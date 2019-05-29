#include "map.h"
#include "thread.h"
#include "machine.h"
#include "utime.h"
#include "unwind.h"
#include <string.h>
#include <assert.h>

struct thread *thread__new(pid_t tgid, pid_t tid)
{
     struct thread *thread = xcalloc(sizeof(*thread), 1);

     thread->tgid = tgid;
     thread->tid = tid;

     refcount_set(&thread->refcnt, 1);
     RB_CLEAR_NODE(&thread->rb_node);

     return thread;
}

void thread__delete(struct thread *thread)
{
     assert(RB_EMPTY_NODE(&thread->rb_node));

     if (thread->maps) {
          maps__put(thread->maps);
          thread->maps = NULL;
     }

     unwind_finish_access(thread);
     free(thread);
}

struct thread *thread__get(struct thread *thread)
{
     if (thread)
          refcount_inc(&thread->refcnt);
     return thread;
}

void thread__put(struct thread *thread)
{
     if (thread && refcount_dec_and_test(&thread->refcnt)) {
          /*
           * Remove it from the dead_threads list, as last reference
           * is gone.
           */
          list_del_init(&thread->node);
          thread__delete(thread);
     }
}

int thread__init_maps(struct thread *thread, struct machine *machine)
{
     pid_t tgid = thread->tgid;

     if (tgid == thread->tid) {
          thread->maps = maps__new(machine);
     } else {
          struct thread *leader = __machine__findnew_thread(machine, tgid, tgid);
          if (leader) {
               thread->maps = maps__get(leader->maps);
               thread__put(leader);
          }
     }

     return thread->maps ? 0 : -1;
}

int thread__insert_map(struct thread *thread, struct map *map)
{
     int ret;

     /* TODO: Is there a better place? */
     ret = unwind_prepare_access(thread, map, NULL);
     if (ret)
          return ret;

     /* TODO: handle overlapping maps? */
     maps__insert(thread->maps, map);

     return 0;
}
