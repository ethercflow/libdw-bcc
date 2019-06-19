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

typedef struct machine machine_t;
struct map;

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

struct dl_phdr_info {
    u64 start_addr;
    u64 end_addr;
    const char *dlpi_name;
};

machine_t *machine__new(void);
int bpf_unwind_ctx__thread_map(machine_t *machine, pid_t tgid, pid_t tid);
int bpf_unwind_ctx__resolve_callchain(struct stacktrace *st,
                                      machine_t *machine,
                                      struct unwind_ctx *uc);
int bpf_dl_iterate_phdr(machine_t *machine, pid_t tgid,
                        int (*__callback)(struct dl_phdr_info *info, void *ctx),
                        void *ctx);
void machine__delete(machine_t *machine);

#ifdef __cplusplus
}
#endif

#endif // __LIBDW_BCC_H_
