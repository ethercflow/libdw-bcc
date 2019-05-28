#ifndef __LIBDW_BCC_H_
#define __LIBDW_BCC_H_

#include "types.h"
#include "ptrace.h"
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TASK_COMM_LEN
# define TASK_COMM_LEN    16
#endif

#ifndef STACK_SIZE
# define STACK_SIZE       4096 * 2
#endif

struct machine;

struct stacktrace {
    int depth;
    u64 *ips;
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

int bpf_unwind_ctx__thread_map(struct machine *machine, pid_t tgid, pid_t tid);
int bpf_unwind_ctx__resolve_callchain(struct stacktrace *st,
                                      struct machine *machine,
                                      struct unwind_ctx *uc);

#ifdef __cplusplus
}
#endif

#endif // __LIBDW_BCC_H_
