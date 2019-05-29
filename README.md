# libdw-bcc

A DWARF-based user-stack unwinding library for bcc.

# interface

``` c
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

machine_t *machine__new(void);
int bpf_unwind_ctx__thread_map(machine_t *machine, pid_t tgid, pid_t tid);
int bpf_unwind_ctx__resolve_callchain(struct stacktrace *st,
                                      machine_t *machine,
                                      struct unwind_ctx *uc);
void machine__delete_threads(machine_t *machine);
void machine__exit(machine_t *machine);

#ifdef __cplusplus
}
#endif
```
