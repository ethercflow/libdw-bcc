#ifndef __DSO_H_
#define __DSO_H_

#include "list.h"
#include "types.h"
#include "rbtree.h"
#include "refcount.h"
#include "rwsem.h"
#include <stdlib.h>

enum dso_data_status {
    DSO_DATA_STATUS_ERROR = -1,
    DSO_DATA_STATUS_UNKNOWN = 0,
    DSO_DATA_STATUS_OK = 1,
};

struct map;
struct machine;

#define DSO__DATA_CACHE_SIZE 4096
#define DSO__DATA_CACHE_MASK ~(DSO__DATA_CACHE_SIZE - 1)

struct dso_cache {
    struct rb_node rb_node;
    u64 offset;
    u64 size;
    char data[0];
};

/*
 * DSOs are put into both a list for fast iteration and rbtree for fast
 * long name lookup.
 */
struct dsos {
    struct list_head head;
    struct rb_root root; /* rbtree root sorted by long name */
    struct rw_semaphore lock;
};

struct dso *dsos__findnew(struct dsos *dsos, const char *name);

struct dso {
    pthread_mutex_t lock;
    struct list_head node;
    struct rb_node   rb_node;    /* rbtree node sorted by long name */
    struct rb_root   *root;      /* root of rbtree that rb_node is in */

    /* dso data file */
    struct {
        struct rb_root cache;
        int fd;
        int status;
        size_t file_size;
        u64 eh_frame_hdr_offset;
    } data;

    const char *short_name;
    const char *long_name;
    u16 long_name_len;
    u16 short_name_len;

    u8 short_name_allocated:1;
    u8 long_name_allocated:1;

    refcount_t refcnt;
    char name[0];
};

struct dso *dso_new(const char *name);
void dso_delete(struct dso *dso);
struct dso *dso__get(struct dso *dso);
void dso__put(struct dso *dso);
struct dso *__dsos__find(struct dsos *dsos,
                         const char *name,
                         bool cmp_short);
struct dso *dsos__find(struct dsos *dsos,
                       const char *name,
                       bool cmp_short);
struct dso *__dsos__addnew(struct dsos *dsos,
                           const char *name);

static inline void __dso__zput(struct dso **dso)
{
    dso__put(*dso);
    *dso = NULL;
}

#define dso__zput(dso) __dso__zput(&dso)

int dso__data_get_fd(struct dso *dso, struct machine *machine);
void dso__data_put_fd(struct dso *dso);

ssize_t dso__data_read_offset(struct dso *dso, struct machine *machine,
                              u64 offset, u8 *data, ssize_t size);
ssize_t dso__data_read_addr(struct dso *dso, struct map *map,
                            struct machine *machine, u64 addr,
                            u8 *data, ssize_t size);

#endif // __DSO_H_
