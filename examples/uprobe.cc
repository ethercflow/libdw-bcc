#include "queue.h"
#include <bcc/BPF.h>
#include <iostream>
#include <fstream>
#include <cinttypes>
#include <csignal>
#include <thread>
#include <libdw_bcc.h>
#include <cstdlib>
#include <elf.h>

std::string BPF_PROGRAM = R"(
#include <linux/sched.h>
#include <uapi/linux/ptrace.h>

#ifndef __inline
# define __inline                               \
        inline __attribute__((always_inline))
#endif

#define STACK_SIZE    4096 * 2

struct key_ {
        u64 ts;
        u64 id;
};

struct unwind_ctx {
        u64 ts;
        u32 tid;
        u32 tgid;

        struct pt_regs uregs;
        char name[TASK_COMM_LEN];

        int size;
        char data[STACK_SIZE];
};

BPF_ARRAY(zero, struct unwind_ctx, 1);
BPF_HASH(cache, struct key_, struct unwind_ctx);

BPF_PERF_OUTPUT(unwind_ctxs);
static __inline int get_unwind_ctx(struct pt_regs *ctx)
{
        struct pt_regs *user_regs = NULL;
        struct task_struct *task = NULL;
        struct unwind_ctx *zuc = NULL;
        struct unwind_ctx *uc = NULL;
        struct key_ k = {};
        void *sp = NULL;
        int ret = 0;
        int z = 0;

        task = (struct task_struct *)bpf_get_current_task();
        if (user_mode(ctx)) {
                user_regs = ctx;
        } else {
                if (task->mm)
                     user_regs = ((struct pt_regs *)(task)->thread.sp0 - 1);
        }

        if (!user_regs)
                return -1;

        ret = bpf_probe_read(&sp, sizeof(sp), &user_regs->sp);
        if (ret < 0)
                return -1;

        zuc = zero.lookup(&z);
        if (!zuc)
                return -1;

        k.ts = bpf_ktime_get_ns();
        k.id = bpf_get_current_pid_tgid();

        uc = cache.lookup_or_init(&k, zuc);
        if (!uc)
                return -1;

        uc->ts = k.ts;
        uc->tgid = k.id >> 32;
        uc->tid = k.id;
        ret = bpf_probe_read(&uc->uregs, sizeof(uc->uregs), user_regs);
        if (ret < 0)
                return -1;
        bpf_get_current_comm(&uc->name, sizeof(uc->name));
        ret = bpf_probe_read_stack(&uc->data, sizeof(uc->data), sp);
        if (ret < 0)
                return -1;
        if (ret == 0)
                uc->size = STACK_SIZE;
        else
                uc->size = STACK_SIZE - ret;

        unwind_ctxs.perf_submit(ctx, uc, sizeof(*uc));

        cache.delete(&k);

        return 0;
}

int probe_func_entry(void *ctx)
{
        if (get_unwind_ctx(ctx) < 0)
                bpf_trace_printk("get_unwind_ctx failed\n");

        return 0;
}
)";

#ifndef __maybe_unused
# define __maybe_unused __attribute__((unused))
#endif

typedef uint64_t    unw_word_t;

static ebpf::BPF *bpf;

static void unwind_ctx_handler(void *cb_cookie,
                               void *raw,
                               int raw_size __maybe_unused) {
    auto uc = static_cast<unwind_ctx*>(raw);
    auto q = static_cast<Queue<unwind_ctx>*>(cb_cookie);
#if 0
    std::cout << std::dec << "TIME: " << uc->ts
              << " TGID: " << uc->tgid << " TID: " << uc->tid
              << " (" << uc->name << ")" << " read stack size: "
              << uc->size << " sp: 0x" << std::hex << uc->uregs.sp
              << std::endl;
#endif
    q->push(*uc);
}

static void signal_handler(int s) {
    std::cerr << "Terminating..." << std::endl;
    delete bpf;
    exit(0);
}

static void kevent_poll(void) {
    while (true) {
        bpf->poll_perf_buffer("unwind_ctxs");
    }
}

static void resolve_callchain(Queue<unwind_ctx>& q, pid_t tgid, pid_t tid)
{
    machine_t *machine = machine__new();
    auto ret = bpf_unwind_ctx__thread_map(machine, tgid, tid);
    if (ret) {
        std::cerr << "thread_map failed: " << ret << std::endl;
    }
    struct stacktrace st;
    st.depth = 4;
    st.ips = reinterpret_cast<u64*>(calloc(sizeof(u64*), st.depth));

    do {
        auto uc = q.pop();
        auto ret = bpf_unwind_ctx__resolve_callchain(&st, machine, &uc);
        if (ret) {
            std::cerr << "resolve_callchain failed: " << ret << std::endl;
            exit(1);
        }
#if 0
        std::cout << "uc.size: " << uc.size << std::endl;
        for (int i = 0; i < st.depth; i++) {
            std::cout << "ip: 0x" << std::hex  << st.ips[i] << std::endl;
        }
        std::string fname = std::to_string(uc.tgid) + "-"
            + std::to_string(uc.tgid) + "-" + std::to_string(uc.ts)
            + "-stack.txt";
        std::ofstream sf(fname, std::ios_base::app);
        auto p = reinterpret_cast<unw_word_t*>(uc.data);
        for (int i = 0; i < uc.size / 8; i++) {
            sf << std::hex << *p << std::endl;
            ++p;
        }
        sf.close();
#endif
        bcc_symbol symbol;
        bcc_symbol_option symbol_option = {
            .use_debug_file = 1,
            .check_debug_file_crc = 1,
            .use_symbol_type = (1 << STT_FUNC) | (1 << STT_GNU_IFUNC)
        };
        void *cache = bcc_symcache_new(tgid, &symbol_option);
        for (int i = 0; i < st.depth; i++) {
            if (bcc_symcache_resolve(cache, st.ips[i], &symbol) != 0) {
                std::cout << "[UNKNOWN]" << std::endl;
            } else {
                std::cout << symbol.demangle_name << std::endl;
                bcc_symbol_free_demangle_name(&symbol);
            }
        }
    } while (true);
}

int main(int argc __maybe_unused, char **argv __maybe_unused) {
    pid_t tgid(std::stoi(argv[1]));
    pid_t tid(std::stoi(argv[2]));
    std::string bin_path(argv[3]);
    std::string probe_func(argv[4]);
    bpf = new ebpf::BPF(0, nullptr, true, "", true);
    auto init_res = bpf->init(BPF_PROGRAM);
    if (init_res.code() != 0) {
        std::cerr << init_res.msg() << std::endl;
        return 1;
    }

    auto attach_res = bpf->attach_uprobe(bin_path, probe_func, "probe_func_entry");
    if (attach_res.code() != 0) {
        std::cerr << attach_res.msg() << std::endl;
        return 1;
    }

    Queue<unwind_ctx> q;
    auto open_res = bpf->open_perf_buffer("unwind_ctxs", &unwind_ctx_handler,
                                          nullptr, reinterpret_cast<void*>(&q), 64);
    if (open_res.code() != 0) {
        std::cerr << open_res.msg() << std::endl;
        return 1;
    }

    if (bpf->free_bcc_memory()) {
        std::cerr << "Failed to free llvm/clang memory" << std::endl;
        return 1;
    }

    signal(SIGINT, signal_handler);
    std::cout << "Started tracing, hit Ctrl-C to terminate." << std::endl;

    std::thread t1(kevent_poll);
    std::thread t2(std::bind(resolve_callchain, std::ref(q), tgid, tid));
    t1.join();
    t2.join();

    auto detach_res = bpf->detach_all();
    if (detach_res.code() != 0) {
        std::cerr << detach_res.msg() << std::endl;
    }

    return 0;
}
