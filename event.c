#include "map.h"
#include "event.h"
#include "thread.h"
#include "machine.h"
#include "utility.h"
#include "unwind_ctx.h"
#include <assert.h>
#include <sys/mman.h>
#include <string.h>
#include <inttypes.h>

int bpf_unwind_ctx_preprocess(struct machine *machine, struct unwind_ctx *uc)
{
    struct thread *thread = machine__findnew_thread(machine, uc->tgid, uc->tid);

    // TODO: set thread comm
    //
    thread__put(thread);

    return 0;
}

int bpf_unwind_ctx__mmap_event(struct mmap2_event *event, struct machine *machine, struct unwind_ctx *uc, bool mmap_data)
{
    char filename[PATH_MAX];
    FILE *fp;
    unsigned long long t;
    int rc = 0;


    snprintf(filename, sizeof(filename), "/proc/%d/task/%d/maps", uc->tgid, uc->tgid);

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
            if (!mmap_data || prot[0] != 'r')
                continue;
        }

        if (!strcmp(execname, ""))
            strcpy(execname, anonstr);

        size = strlen(execname) + 1;
        memcpy(event->filename, execname, size);
        size = ALIGN(size, sizeof(u64));
        event->len -= event->start;
        event->tgid = uc->tgid;
        event->tid = uc->tid;
    }

    fprintf(stderr, "handle map_event cost: %llu\n", rdclock() - t);

    fclose(fp);
    return rc;
}

int bpf_unwind_ctx__process_mmap_event(struct machine *machine,
                                       struct mmap2_event *event,
                                       struct unwind_ctx *uc)
{
    struct thread *thread;
    struct map *map;

    thread = machine__findnew_thread(machine, event->tgid, event->tid);
    assert(!thread);

    map = map__new(machine, thread, event);

//     map = map__new(machine, event->start,
//                    event->len, event->pgoff,
//                    event->tgid, event->maj,
//                    event->min, event->ino,
//                    event->ino_generation,
//                    event->prot,
//                    event->flags,
//                    event->filename,
//                    thread);


}
