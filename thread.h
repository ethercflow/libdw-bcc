#ifndef __THREAD_H_
#define __THREAD_H_

#include "rbtree.h"
#include "list.h"
#include "refcount.h"

#ifndef TASK_COMM_LEN
# define TASK_COMM_LEN    16
#endif

struct maps;
struct unwind_libunwind_ops;

struct thread {
        union {
                struct rb_node rb_node;
                list_t node;
        };
        struct maps *maps;
        pid_t tgid;
        pid_t tid;
        char name[TASK_COMM_LEN];
        void *addr_space;
        struct unwind_libunwind_ops *ulops;
        refcount_t refcnt;
};

struct thread *thread_new(pid_t tgid, pid_t tid, const char *name);
int thread_init_maps(struct thread *thread, void *ctx);
void thread_delete(struct thread *thread);


#endif // __THREAD_H_
