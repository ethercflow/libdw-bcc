#include <linux/sched.h>
#include <uapi/linux/ptrace.h>

#ifndef __inline
# define __inline                               \
        inline __attribute__((always_inline))
#endif

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

static __inline
int get_unwind_ctx(struct pt_regs *ctx, bool tracepoint, void *attr)
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
        if (!tracepoint && ctx && user_mode(ctx)) {
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
        if (!tracepoint && ctx && !user_mode(ctx)) {
                sp -= 16;
                uc->uregs.sp = (unsigned long)sp;
        }
        ret = bpf_probe_read_stack(&uc->data, sizeof(uc->data), sp);
        if (ret < 0)
                return -1;
        if (ret == 0)
                uc->size = STACK_SIZE;
        else
                uc->size = STACK_SIZE - ret;

        if (tracepoint)
                unwind_ctxs.perf_submit(attr, uc, sizeof(*uc));
        else
                unwind_ctxs.perf_submit(ctx, uc, sizeof(*uc));

        cache.delete(&k);

        return 0;
}
