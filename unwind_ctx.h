#ifndef __UNWIND_CTX_H_
#define __UNWIND_CTX_H_

#include "ptrace.h"

#ifndef TASK_COMM_LEN
# define TASK_COMM_LEN    16
#endif

#ifndef STACK_SIZE
# define STACK_SIZE       4096 * 2
#endif

// TODO:
// 1. how to get dso's name?
// 2. add a flag to distinguish exec and dso
struct unwind_ctx {
    u64 ts;
    u32 tid;
    u32 tgid;

    struct pt_regs uregs;
    char name[TASK_COMM_LEN];

    int size;
    char data[STACK_SIZE];
};

#endif // __UNWIND_CTX_H_
