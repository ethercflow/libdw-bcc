#include "machine.h"
#include "thread.h"
#include "map.h"
#include "rbtree.h"
#include <string.h>
#include <assert.h>

static void dsos__init(struct dsos *dsos)
{
    INIT_LIST_HEAD(&dsos->head);
    dsos->root = RB_ROOT;
    init_rwsem(&dsos->lock);
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

int machine__init(struct machine *machine)
{
    memset(machine, 0, sizeof(*machine));
    dsos__init(&machine->dsos);
    machine__threads_init(machine);

    return 0;
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

static inline struct threads *machine__threads(struct machine *machine, pid_t tid)
{
    /* Cast it to handle tid == -1 */
    return &machine->threads[(unsigned int)tid % THREADS__TABLE_SIZE];
}

struct thread *__machine__findnew_thread(struct machine *machine, pid_t tgid, pid_t tid)
{
    return ____machine__findnew_thread(machine, machine__threads(machine, tid), tgid, tid, true);
}

struct thread *
machine__findnew_thread(struct machine *machine, pid_t tgid, pid_t tid)
{
    struct threads *threads = machine__threads(machine, tid);
    struct thread *th;

    down_write(&threads->lock);
    th = __machine__findnew_thread(machine, tgid, tid);
    up_write(&threads->lock);

    return NULL;
}

struct dso *machine__findnew_dso(struct machine *machine, const char *fname)
{
    return dsos__findnew(&machine->dsos, fname);
}
