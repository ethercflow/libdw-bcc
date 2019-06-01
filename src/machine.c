#include "machine.h"
#include "thread.h"
#include "libdw_bpf.h"
#include "map.h"
#include "rbtree.h"
#include <string.h>
#include <assert.h>

#ifdef debug
#undef debug
#define debug(args...)    ""
#endif

static void dsos__init(struct dsos *dsos)
{
    INIT_LIST_HEAD(&dsos->head);
    dsos->root = RB_ROOT;
    init_rwsem(&dsos->lock);
}

static void dsos__purge(struct dsos *dsos)
{
    struct dso *pos, *n;

    down_write(&dsos->lock);

    list_for_each_entry_safe(pos, n, &dsos->head, node) {
        RB_CLEAR_NODE(&pos->rb_node);
        pos->root = NULL;
        list_del_init(&pos->node);
        dso__put(pos);
    }

    up_write(&dsos->lock);
}

static void dsos__exit(struct dsos *dsos)
{
    dsos__purge(dsos);
    exit_rwsem(&dsos->lock);
}

static void machine__threads_init(struct machine *machine)
{
    for (int i = 0; i < THREADS__TABLE_SIZE; i++) {
        struct threads *threads = &machine->threads[i];
        threads->entries = RB_ROOT;
        init_rwsem(&threads->lock);
        threads->nr = 0;
        INIT_LIST_HEAD(&threads->dead);
        threads->last_match = NULL;
    }
}

static inline
struct threads *machine__threads(struct machine *machine, pid_t tid)
{
    /* Cast it to handle tid == -1 */
    return &machine->threads[(unsigned int)tid % THREADS__TABLE_SIZE];
}

static void __machine__remove_thread(struct machine *machine, struct thread *th,
                                     bool lock)
{
     struct threads *threads = machine__threads(machine, th->tid);

     if (threads->last_match == th)
          threads->last_match = NULL;

     assert(refcount_read(&th->refcnt) != 0);
     if (lock)
          down_write(&threads->lock);
     rb_erase_init(&th->rb_node, &threads->entries);
     RB_CLEAR_NODE(&th->rb_node);
     --threads->nr;
     /*
      * Move it first to the dead_threads list, then drop the reference,
      * if this is the last reference, then the thread__delete destructor
      * will be called and we will remove it from the dead_threads list.
      */
     list_add_tail(&th->node, &threads->dead);
     if (lock)
          up_write(&threads->lock);
     thread__put(th);
}

static void machine__delete_threads(struct machine *machine)
{
     struct rb_node *nd;
     int i;

     for (i = 0; i < THREADS__TABLE_SIZE; i++) {
          struct threads *threads = &machine->threads[i];
          down_write(&threads->lock);
          nd = rb_first(&threads->entries);
          while (nd) {
               struct thread *t = rb_entry(nd, struct thread, rb_node);

               nd = rb_next(nd);
               __machine__remove_thread(machine, t, false);
          }
          up_write(&threads->lock);
     }
}

static void machine__exit(struct machine *machine)
{
    int i;

    if (machine == NULL)
        return;

    dsos__exit(&machine->dsos);

    for (i = 0; i < THREADS__TABLE_SIZE; i++) {
        struct threads *threads = &machine->threads[i];
        exit_rwsem(&threads->lock);
    }
}

void machine__delete(struct machine *machine)
{
    if (machine) {
        machine__delete_threads(machine);
        machine__exit(machine);
        free(machine);
    }
}

struct machine *machine__new(void)
{
    struct machine *machine = xmalloc(sizeof(*machine));
    machine__init(machine);
    return machine;
}

void machine__init(struct machine *machine)
{
    memset(machine, 0, sizeof(*machine));
    dsos__init(&machine->dsos);
    machine__threads_init(machine);
}

static void machine__update_thread_tgid(struct machine *machine,
                                       struct thread *th, pid_t tgid)
{
    struct thread *leader;

    if (tgid == th->tgid)
        return;

    th->tgid = tgid;

    if (th->tgid == th->tid)
        return;

    leader = __machine__findnew_thread(machine, th->tgid, th->tgid);
    if (!leader)
        goto out_err;

    if (!leader->maps)
        leader->maps = maps__new(machine);

    if (!leader->maps)
        goto out_err;

    if (th->maps == leader->maps)
        return;

    if (th->maps) {
        if (!maps__empty(th->maps))
            assert(0);
        maps__put(th->maps);
    }

    th->maps = maps__get(leader->maps);

    thread__put(leader);
    return;

out_err:
    assert(0);

    return;
}

static struct thread *____machine__findnew_thread(struct machine *machine,
                                                  struct threads *threads,
                                                  pid_t tgid, pid_t tid,
                                                  bool create)
{
    struct rb_node **p = &threads->entries.rb_node;
    struct rb_node *parent = NULL;
    struct thread *th;

    /*
     * Front-end cache - TID lookups come in blocks,
     * so most of the time we dont have to look up
     * the full rbtree:
     */
    th = threads->last_match;
    if (th != NULL) {
        if (th->tid == tid) {
            machine__update_thread_tgid(machine, th, tgid);
            return thread__get(th);
        }

        threads->last_match = NULL;
    }

    while (*p != NULL) {
        parent = *p;
        th = rb_entry(parent, struct thread, rb_node);

        if (th->tid == tid) {
            threads->last_match = th;
            machine__update_thread_tgid(machine, th, tgid);
            return thread__get(th);
        }

        if (tid < th->tid)
            p = &(*p)->rb_left;
        else
            p = &(*p)->rb_right;
    }

    if (!create)
        return NULL;

    th = thread__new(tgid, tid);
    debug("____machine__findnew_thread, tgid: %d, tid: %d\n", tgid, tid);
    if (th != NULL) {
        rb_link_node(&th->rb_node, parent, p);
        rb_insert_color(&th->rb_node, &threads->entries);

        /*
         * We have to initialize maps separately
         * after rb tree is updated.
         *
         * The reason is that we call machine__findnew_thread
         * within thread__init_maps to find the thread
         * leader and that would screwed the rb tree.
         */
        if (thread__init_maps(th, machine)) {
            debug("clear thread\n");
            rb_erase_init(&th->rb_node, &threads->entries);
            RB_CLEAR_NODE(&th->rb_node);
            thread__put(th);
            return NULL;
        }
        /*
         * It is now in the rbtree, get a ref
         */
        thread__get(th);
        threads->last_match = th;
        ++threads->nr;
    }

    return th;
}

struct thread *__machine__findnew_thread(struct machine *machine,
                                         pid_t tgid, pid_t tid)
{
    return ____machine__findnew_thread(machine, machine__threads(machine, tid),
                                       tgid, tid, true);
}

struct thread *
machine__findnew_thread(struct machine *machine, pid_t tgid, pid_t tid)
{
    struct threads *threads = machine__threads(machine, tid);
    struct thread *th;

    down_write(&threads->lock);
    th = __machine__findnew_thread(machine, tgid, tid);
    up_write(&threads->lock);

    return th;
}

struct dso *machine__findnew_dso(struct machine *machine, const char *fname)
{
    return dsos__findnew(&machine->dsos, fname);
}
