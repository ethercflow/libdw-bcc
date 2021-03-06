#include <bcc/BPF.h>
#include <iostream>
#include <fstream>
#include <cinttypes>
#include <csignal>

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

int on_sys_clone(void *ctx)
{
        if (get_unwind_ctx(ctx) < 0)
                bpf_trace_printk("get_unwind_ctx failed\n");

        return 0;
}
)";

#ifndef __maybe_unused
# define __maybe_unused __attribute__((unused))
#endif

#define TASK_COMM_LEN    16
#define STACK_SIZE       4096 * 2

typedef uint64_t    u64;
typedef uint32_t    u32;
typedef uint64_t    unw_word_t;

struct pt_regs {
/*
 * C ABI says these regs are callee-preserved. They aren't saved on kernel entry
 * unless syscall needs a complete, fully filled "struct pt_regs".
 */
    unsigned long r15;
    unsigned long r14;
    unsigned long r13;
    unsigned long r12;
    unsigned long bp;
    unsigned long bx;
/* These regs are callee-clobbered. Always saved on kernel entry. */
    unsigned long r11;
    unsigned long r10;
    unsigned long r9;
    unsigned long r8;
    unsigned long ax;
    unsigned long cx;
    unsigned long dx;
    unsigned long si;
    unsigned long di;
/*
 * On syscall entry, this is syscall#. On CPU exception, this is error code.
 * On hw interrupt, it's IRQ number:
 */
    unsigned long orig_ax;
/* Return frame for iretq */
    unsigned long ip;
    unsigned long cs;
    unsigned long flags;
    unsigned long sp;
    unsigned long ss;
/* top of stack page */
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

static void unwind_ctx_handler(void *cb_cookie __maybe_unused,
                               void *raw,
                               int raw_size __maybe_unused) {
    auto uc = static_cast<unwind_ctx*>(raw);
    std::cout << std::dec << "TIME: " << uc->ts
              << " TGID: " << uc->tgid << " TID: " << uc->tid
              << " (" << uc->name << ")" << " read stack size: "
              << uc->size << " sp: 0x" << std::hex << uc->uregs.sp
              << std::endl;
    std::string fname = std::to_string(uc->tgid) + "-"
        + std::to_string(uc->tgid) + "-" + std::to_string(uc->ts)
        + "-stack.txt";
    std::ofstream sf(fname, std::ios_base::app);
    auto p = reinterpret_cast<unw_word_t*>(uc->data);
    for (int i = 0; i < uc->size / 8; i++) {
        sf << std::hex << *p << std::endl;
        ++p;
    }
    sf.close();
}

static ebpf::BPF *bpf;

static void signal_handler(int s) {
    std::cerr << "Terminating..." << std::endl;
    delete bpf;
    exit(0);
}

int main(int argc __maybe_unused, char **argv __maybe_unused) {
    bpf = new ebpf::BPF(0, nullptr, true, "", true);
    auto init_res = bpf->init(BPF_PROGRAM);
    if (init_res.code() != 0) {
        std::cerr << init_res.msg() << std::endl;
        return 1;
    }

    std::string clone_fnname = bpf->get_syscall_fnname("clone");
    auto attach_res = bpf->attach_kprobe(clone_fnname, "on_sys_clone");
    if (attach_res.code() != 0) {
        std::cerr << attach_res.msg() << std::endl;
        return 1;
    }

    auto open_res = bpf->open_perf_buffer("unwind_ctxs", &unwind_ctx_handler,
                                          nullptr, nullptr, 64);
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
    while (true) {
        bpf->poll_perf_buffer("unwind_ctxs");
    }

    return 0;
}
