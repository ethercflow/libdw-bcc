#ifndef __THREAD_H_
#define __THREAD_H_

#include "rbtree.h"
#include "list.h"
#include "refcount.h"

#ifndef TASK_COMM_LEN
# define TASK_COMM_LEN    16
#endif

struct maps;
struct machine;
struct unwind_libunwind_ops;

struct thread_leader {
     struct rb_root entries;
     struct list_head node;
};

struct thread {
     union {
          struct rb_node rb_node;
          struct list_head node;
     };
     struct maps *maps;
     pid_t tgid;
     pid_t tid;
     char name[TASK_COMM_LEN];
     void *addr_space;
     struct unwind_libunwind_ops *ulops;
     refcount_t refcnt;
};

struct thread *thread__new(pid_t tgid, pid_t tid);
void thread__delete(struct thread *thread);
struct thread *thread__get(struct thread *thread);
void thread__put(struct thread *thread);
int thread__init_maps(struct thread *thread, struct machine *machine);

#endif // __THREAD_H_
