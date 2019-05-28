#include "map.h"
#include "thread.h"
#include "machine.h"
#include "utility.h"
#include "event.h"
#include "libdw_bcc.h"
#include "unwind.h"
#include <assert.h>
#include <sys/mman.h>
#include <string.h>
#include <inttypes.h>

typedef int (*event__handler_t)(struct machine *machine,
                                struct mmap2_event *event);

static int bpf_unwind_ctx__process_mmap(struct machine *machine,
                                       struct mmap2_event *event)
{
    struct thread *thread;
    struct map *map;

    thread = machine__findnew_thread(machine, event->tgid, event->tid);
    assert(thread != NULL);

    map = map__new(machine, thread, event);

    assert(!thread__insert_map(thread, map));
    thread__put(thread);
    map__put(map);

    return 0;
}

static int bpf_unwind_ctx_prepare_mmap(struct machine *machine,
                                       struct mmap2_event *event,
                                       pid_t tgid, pid_t tid,
                                       event__handler_t process)
{
    char filename[PATH_MAX];
    FILE *fp;
    unsigned long long t;
    int rc = 0;

    snprintf(filename, sizeof(filename), "/proc/%d/task/%d/maps", tgid, tgid);

    fp = fopen(filename, "r");
    if (fp == NULL) {
        /*
         * We raced with a task exiting - just return:
         */
        fprintf(stderr, "couldn't open %s\n", filename);
        return -1;
    }

    t = rdclock();

    while (1) {
        char bf[BUFSIZ];
        char prot[5];
        char execname[PATH_MAX];
        char anonstr[] = "//anon";
        unsigned int ino;
        size_t size;
        ssize_t n;

        if (fgets(bf, sizeof(bf), fp) == NULL)
            break;

        /* ensure null termination since stack will be reused. */
        strcpy(execname, "");

        /* 00400000-0040c000 r-xp 00000000 fd:01 41038  /bin/cat */
        n = sscanf(bf, "%"PRIx64"-%"PRIx64" %s %"PRIx64" %x:%x %u %[^\n]\n",
                   &event->start, &event->len, prot,
                   &event->pgoff, &event->maj,
                   &event->min,
                   &ino, execname);

        /*
         * Anon maps don't have the execname.
         */
        if (n < 7)
            continue;

        event->ino = (u64)ino;

        /* map protection and flags bits */
        event->prot = 0;
        event->flags = 0;
        if (prot[0] == 'r')
            event->prot |= PROT_READ;
        if (prot[1] == 'w')
            event->prot |= PROT_WRITE;
        if (prot[2] == 'x')
            event->prot |= PROT_EXEC;

        if (prot[3] == 's')
            event->flags |= MAP_SHARED;
        else
            event->flags |= MAP_PRIVATE;

        if (prot[2] != 'x') {
            if (prot[0] != 'r')
                continue;
        }

        if (!strcmp(execname, ""))
            strcpy(execname, anonstr);

        size = strlen(execname) + 1;
        memcpy(event->filename, execname, size);
        size = ALIGN(size, sizeof(u64));
        event->len -= event->start;
        event->tgid = tgid;
        event->tid = tid;

        process(machine, event);
    }

    fprintf(stderr, "handle map_event cost: %llu\n", rdclock() - t);

    fclose(fp);

    return rc;
}

int bpf_unwind_ctx__thread_map(struct machine *machine, pid_t tgid, pid_t tid)
{
    struct mmap2_event *event;
    int ret = 0;

    event = xmalloc(sizeof(*event));
    ret = bpf_unwind_ctx_prepare_mmap(machine, event, tgid, tid,
                                      bpf_unwind_ctx__process_mmap);
    free(event);

    return ret;
}

int bpf_unwind_ctx__resolve_callchain(struct stacktrace *st,
                                      struct machine *machine,
                                      struct unwind_ctx *uc)
{
    struct thread *thread;

    thread = machine__findnew_thread(machine, uc->tgid, uc->tid);
    assert(thread != NULL);

    return unwind_get_entries(NULL, NULL, thread, uc, st);
}
