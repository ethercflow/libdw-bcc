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

#endif // __EVENT_H_
