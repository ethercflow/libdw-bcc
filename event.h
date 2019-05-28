#ifndef __EVENT_H_
#define __EVENT_H_

#include "types.h"
#include "utility.h"

struct unwind_ctx;
struct machine;

struct mmap2_event {
    u32 tgid, tid;
    u64 start;
    u64 len;
    u64 pgoff;
    u32 maj;
    u32 min;
    u64 ino;
    u64 ino_generation;
    u32 prot;
    u32 flags;
    char filename[PATH_MAX];
};

#ifdef __cplusplus
extern "C" {
#endif

int bpf_unwind_ctx__comm_event(struct machine *machine, struct unwind_ctx *uc);
int bpf_unwind_ctx__mmap_event(struct machine *machine, struct unwind_ctx *uc);
int bpf_unwind_ctx__process_mmap_event(struct machine *machine,
                                       struct mmap2_event *event,
                                       struct unwind_ctx *uc);
#ifdef __cplusplus
}
#endif

#endif // __EVENT_H_
