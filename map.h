#ifndef __MAP_H_
#define __MAP_H_

#include "rbtree.h"
#include "list.h"
#include "types.h"
#include "rwsem.h"
#include "refcount.h"

struct dso;
struct maps;

struct map {
        union {
                struct rb_node rb_node;
                struct list_head node;
        };
        u64 start;
        u64 end;
        u8 type;
        u32 priv;
        u32 prot;
        u32 flags;
        u64 pgoff;
        u64 reloc;
        u32 maj, min;
        u64 ino;
        u64 ino_generation;

        /* ip -> dso rip */
        u64 (*map_ip)(struct map *, u64);
        /* dso rip -> ip */
        u64 (*unmap_ip)(struct map *, u64);

        struct dso *dso;
        struct maps *maps;
        refcount_t refcnt;
};

struct maps {
        struct rb_root entries;
        struct rw_semaphore lock;
};

#endif // __MAP_H_
