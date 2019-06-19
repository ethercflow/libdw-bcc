# libdw-bpf

A DWARF-based user-stack unwinding library for bpf.

Currently the BPF_CALL `bpf_get_stackid` traverses the frame list through the fp
register to implement user-stack unwinding, which is very efficient. But on
x86_64 platform, the frame pointer is used only in cases where the stack frame
may be of variable size, this situation makes `bpf_get_stackid` useless, so
this project born.

## Installing

See [INSTALL.md](INSTALL.md) for installation steps on your platform.

## Interface

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
```

## Usage
### Get frames
1. Call `machine__new` to get a machine_t object
2. call `bpf_unwind_ctx__thread_map` to get a process's address space
   information and manage DSOs (include the process's binary) info. It's only
   need to be called once for each process (tgid), other threads of the process
   will share these info with the tgid
3. Write eBPF code to handle events and call
   [get_unwind_ctx](bpf/ebpf_get_unwind_ctx.c) to create and pass`unwind_ctx` objs
   to the perf ring buffer
4. Call `bpf_unwind_ctx__reslove_callchain` to get frames

### Get symbol name
We can use the [libbcc](http://github.com/iovisor/bcc):

Call `bcc_symcache_new` and `bpf_symbol_symcache_resolve` to get symbol name

### Cleanup
Call `machine__delete` to release resources

## Examples
- [uprobe event](examples/uprobe.cc)
- [kprobe event](examples/syscall.cc)
- [tracepoint event](examples/pwrite64_event.cc)
