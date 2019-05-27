#ifndef __MACHINE_H_
#define __MACHINE_H_

#include "rbtree.h"
#include "list.h"
#include "rwsem.h"
#include "dso.h"

#define THREADS__TABLE_BITS    8
#define THREADS__TABLE_SIZE    (1 << THREADS__TABLE_BITS)

struct threads {
    struct rb_root entries;
    struct rw_semaphore lock;
    unsigned int nr;
    struct list_head  dead;
    struct thread *last_match;
};

struct machine {
    struct threads threads[THREADS__TABLE_SIZE];
    struct dsos dsos;
};

int machine__init(struct machine *machine);

struct thread *
__machine__findnew_thread(struct machine *machine, pid_t pid, pid_t tid);
struct thread *
machine__findnew_thread(struct machine *machine, pid_t tgid, pid_t tid);
struct dso *machine__findnew_dso(struct machine *machine, const char *fname);

#endif // __MACHINE_H_
