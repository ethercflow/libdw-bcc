#include "dso.h"
#include "map.h"
#include "list.h"
#include "rbtree.h"
#include "symbol.h"
#include "utility.h"
#include <string.h>
#include <pthread.h>
#include <libgen.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

/*
 * Find a matching entry and/or link current entry to RB tree.
 * Either one of the dso or name parameter must be non-NULL or the
 * function will not work.
 */
static struct dso *__dso__findlink_by_longname(struct rb_root *root,
                                               struct dso *dso, const char *name)
{
     struct rb_node **p = &root->rb_node;
     struct rb_node  *parent = NULL;

     if (!name)
          name = dso->long_name;
     /*
      * Find node with the matching name
      */
     while (*p) {
          struct dso *this = rb_entry(*p, struct dso, rb_node);
          int rc = strcmp(name, this->long_name);

          parent = *p;
          if (rc == 0) {
               /*
                * In case the new DSO is a duplicate of an existing
                * one, print a one-time warning & put the new entry
                * at the end of the list of duplicates.
                */
               if (!dso || (dso == this))
                    return this;    /* Find matching dso */
               /*
                * The core kernel DSOs may have duplicated long name.
                * In this case, the short name should be different.
                * Comparing the short names to differentiate the DSOs.
                */
               rc = strcmp(dso->short_name, this->short_name);
               if (rc == 0) {
                    fprintf(stderr, "Duplicated dso name: %s\n", name);
                    return NULL;
               }
          }
          if (rc < 0)
               p = &parent->rb_left;
          else
               p = &parent->rb_right;
     }
     if (dso) {
          /* Add new node and rebalance tree */
          rb_link_node(&dso->rb_node, parent, p);
          rb_insert_color(&dso->rb_node, root);
          dso->root = root;
     }
     return NULL;
}

static inline struct dso *__dso__find_by_longname(struct rb_root *root,
                                                  const char *name)
{
     return __dso__findlink_by_longname(root, NULL, name);
}

void dso__set_long_name(struct dso *dso, const char *name, bool name_allocated)
{
     struct rb_root *root = dso->root;

     if (name == NULL)
          return;

     if (dso->long_name_allocated)
          free((char *)dso->long_name);

     if (root) {
          rb_erase(&dso->rb_node, root);
          /*
           * __dso__findlink_by_longname() isn't guaranteed to add it
           * back, so a clean removal is required here.
           */
          RB_CLEAR_NODE(&dso->rb_node);
          dso->root = NULL;
     }

     dso->long_name           = name;
     dso->long_name_len       = strlen(name);
     dso->long_name_allocated = name_allocated;

     if (root)
          __dso__findlink_by_longname(root, dso, NULL);
}

void dso__set_short_name(struct dso *dso, const char *name, bool name_allocated)
{
     if (name == NULL)
          return;

     if (dso->short_name_allocated)
          free((char *)dso->short_name);

     dso->short_name           = name;
     dso->short_name_len       = strlen(name);
     dso->short_name_allocated = name_allocated;
}

static void dso__set_basename(struct dso *dso)
{
       /*
        * basename() may modify path buffer, so we must pass
        * a copy.
        */
       char *base, *lname = strdup(dso->long_name);

       if (!lname)
               return;

       /*
        * basename() may return a pointer to internal
        * storage which is reused in subsequent calls
        * so copy the result.
        */
       base = strdup(basename(lname));

       free(lname);

       if (!base)
               return;

       dso__set_short_name(dso, base, true);
}

struct dso *dso__new(const char *name)
{
     struct dso *dso = xcalloc(1, sizeof(*dso) + strlen(name) + 1);

     strcpy(dso->name, name);
     dso__set_long_name(dso, dso->name, false);
     dso__set_short_name(dso, dso->name, false);
     dso->data.cache = RB_ROOT;
     dso->data.fd = -1;
     dso->data.status = DSO_DATA_STATUS_UNKNOWN;
     RB_CLEAR_NODE(&dso->rb_node);
     dso->root = NULL;
     INIT_LIST_HEAD(&dso->node);
     pthread_mutex_init(&dso->lock, NULL);
     refcount_set(&dso->refcnt, 1);

     return dso;
}

void dso__delete(struct dso *dso)
{

}

struct dso *dso__get(struct dso *dso)
{
     if (dso)
          refcount_inc(&dso->refcnt);
     return dso;
}

void dso__put(struct dso *dso)
{
     if (dso && refcount_dec_and_test(&dso->refcnt))
          dso__delete(dso);
}

void dso__read_binary_type_filename(const struct dso *dso, char *filename, size_t size)
{
     __symbol__join_symfs(filename, size, dso->long_name);
}

static int do_open(char *name)
{
     int fd;

     fd = open(name, O_RDONLY);
     if (fd >= 0)
          return fd;

     /* FIXME: not thread-safe */
     fprintf(stderr, "dso open failed: %s\n", strerror(errno));

     assert(0);

     return -1;
}

/**
 * dso_close - Open DSO data file
 * @dso: dso object
 *
 * Open @dso's data file descriptor and updates
 * list/count of open DSO objects.
 */
static int open_dso(struct dso *dso)
{
     int fd = -EINVAL;
     char *name = xmalloc(PATH_MAX);

     dso__read_binary_type_filename(dso, name, PATH_MAX);

     assert(is_regular_file(name));

     fd = do_open(name);

     free(name);
     return fd;
}

static void try_to_open_dso(struct dso *dso)
{
     if (dso->data.fd >= 0)
          return;

     dso->data.fd = open_dso(dso);

     if (dso->data.fd >= 0)
          dso->data.status = DSO_DATA_STATUS_OK;
     else
          dso->data.status = DSO_DATA_STATUS_ERROR;
}

/**
 * dso__data_get_fd - Get dso's data file descriptor
 * @dso: dso object
 * @machine: machine object
 *
 * External interface to find dso's file, open it and
 * returns file descriptor.  It should be paired with
 * dso__data_put_fd() if it returns non-negative value.
 */
int dso__data_get_fd(struct dso *dso, struct machine *machine __maybe_unused)
{
     if (dso->data.status == DSO_DATA_STATUS_ERROR)
          return -1;

     try_to_open_dso(dso);

     return dso->data.fd;
}

void dso__data_put_fd(struct dso *dso __maybe_unused)
{
}

static struct dso_cache *dso_cache__find(struct dso *dso, u64 offset)
{
     const struct rb_root *root = &dso->data.cache;
     struct rb_node * const *p = &root->rb_node;
     const struct rb_node *parent = NULL;
     struct dso_cache *cache;

     while (*p != NULL) {
          u64 end;

          parent = *p;
          cache = rb_entry(parent, struct dso_cache, rb_node);
          end = cache->offset + DSO__DATA_CACHE_SIZE;

          if (offset < cache->offset)
               p = &(*p)->rb_left;
          else if (offset >= end)
               p = &(*p)->rb_right;
          else
               return cache;
     }

     return NULL;
}

static struct dso_cache *
dso_cache__insert(struct dso *dso, struct dso_cache *new)
{
     struct rb_root *root = &dso->data.cache;
     struct rb_node **p = &root->rb_node;
     struct rb_node *parent = NULL;
     struct dso_cache *cache;
     u64 offset = new->offset;

     pthread_mutex_lock(&dso->lock);
     while (*p != NULL) {
          u64 end;

          parent = *p;
          cache = rb_entry(parent, struct dso_cache, rb_node);
          end = cache->offset + DSO__DATA_CACHE_SIZE;

          if (offset < cache->offset)
               p = &(*p)->rb_left;
          else if (offset >= end)
               p = &(*p)->rb_right;
          else
               goto out;
     }

     rb_link_node(&new->rb_node, parent, p);
     rb_insert_color(&new->rb_node, root);

     cache = NULL;
out:
     pthread_mutex_unlock(&dso->lock);
     return cache;
}

static ssize_t
dso_cache__memcpy(struct dso_cache *cache, u64 offset,
                  u8 *data, u64 size)
{
     u64 cache_offset = offset - cache->offset;
     u64 cache_size   = min(cache->size - cache_offset, size);

     memcpy(data, cache->data + cache_offset, cache_size);
     return cache_size;
}

static ssize_t
dso_cache__read(struct dso *dso, u64 offset, u8 *data, ssize_t size)
{
     struct dso_cache *cache;
     struct dso_cache *old;
     ssize_t ret;

     do {
          u64 cache_offset;

          cache = xmalloc(sizeof(*cache) + DSO__DATA_CACHE_SIZE);
          if (!cache)
               return -ENOMEM;

          /*
           * dso->data.fd might be closed if other thread opened another
           * file (dso) due to open file limit (RLIMIT_NOFILE).
           * TODO: support close file because of RLIMIT_NOFILE
           */
          try_to_open_dso(dso);

          if (dso->data.fd < 0) {
               ret = -errno;
               dso->data.status = DSO_DATA_STATUS_ERROR;
               break;
          }

          cache_offset = offset & DSO__DATA_CACHE_MASK;

          ret = pread(dso->data.fd, cache->data, DSO__DATA_CACHE_SIZE, cache_offset);
          if (ret <= 0)
               break;

          cache->offset = cache_offset;
          cache->size   = ret;
     } while (0);

     if (ret > 0) {
          old = dso_cache__insert(dso, cache);
          if (old) {
               /* we lose the race */
               free(cache);
               cache = old;
          }

          ret = dso_cache__memcpy(cache, offset, data, size);
     }

     if (ret <= 0)
          free(cache);

     return ret;
}

static ssize_t
dso_cache_read(struct dso *dso, u64 offset, u8 *data, ssize_t size)
{
     struct dso_cache *cache;

     cache = dso_cache__find(dso, offset);
     if (cache)
          return dso_cache__memcpy(cache, offset, data, size);
     else
          return dso_cache__read(dso, offset, data, size);
}

/*
 * Reads and caches dso data DSO__DATA_CACHE_SIZE size chunks
 * in the rb_tree. Any read to already cached data is served
 * by cached data.
 */
static ssize_t cached_read(struct dso *dso, u64 offset, u8 *data, ssize_t size)
{
     ssize_t r = 0;
     u8 *p = data;

     do {
          ssize_t ret;

          ret = dso_cache_read(dso, offset, p, size);
          if (ret < 0)
               return ret;

          /* Reached EOF, return what we have. */
          if (!ret)
               break;

          assert(ret <= size);

          r      += ret;
          p      += ret;
          offset += ret;
          size   -= ret;

     } while (size);

     return r;
}

static int data_file_size(struct dso *dso)
{
     int ret = 0;
     struct stat st;

     if (dso->data.file_size)
          return 0;

     if (dso->data.status == DSO_DATA_STATUS_ERROR)
          return -1;

     /*
      * dso->data.fd might be closed if other thread opened another
      * file (dso) due to open file limit (RLIMIT_NOFILE).
      */
     try_to_open_dso(dso);

     if (dso->data.fd < 0) {
          ret = -errno;
          dso->data.status = DSO_DATA_STATUS_ERROR;
          goto out;
     }

     if (fstat(dso->data.fd, &st) < 0) {
          ret = -errno;
          // FIXME: strerror not thread-safe
          fprintf(stderr, "dso cache fstat failed: %s\n", strerror(errno));
          dso->data.status = DSO_DATA_STATUS_ERROR;
          goto out;
     }
     dso->data.file_size = st.st_size;

out:
     return ret;
}

static ssize_t
data_read_offset(struct dso *dso, u64 offset, u8 *data, ssize_t size)
{
     if (data_file_size(dso))
          return -1;

     /* Check the offset sanity. */
     if (offset > dso->data.file_size)
          return -1;

     if (offset + size < offset)
          return -1;

     return cached_read(dso, offset, data, size);
}

/**
 * dso__data_read_offset - Read data from dso file offset
 * @dso: dso object
 * @machine: machine object
 * @offset: file offset
 * @data: buffer to store data
 * @size: size of the @data buffer
 *
 * External interface to read data from dso file offset. Open
 * dso data file and use cached_read to get the data.
 */
ssize_t dso__data_read_offset(struct dso *dso,
                              struct machine *machine __maybe_unused,
                              u64 offset, u8 *data, ssize_t size)
{
     if (dso->data.status == DSO_DATA_STATUS_ERROR)
          return -1;

     return data_read_offset(dso,offset, data, size);
}

/**
 * dso__data_read_addr - Read data from dso address
 * @dso: dso object
 * @machine: machine object
 * @add: virtual memory address
 * @data: buffer to store data
 * @size: size of the @data buffer
 *
 * External interface to read data from dso address.
 */
ssize_t dso__data_read_addr(struct dso *dso, struct map *map,
                            struct machine *machine, u64 addr,
                            u8 *data, ssize_t size)
{
     u64 offset = map->map_ip(map, addr);
     return dso__data_read_offset(dso, machine, offset, data, size);
}

void __dsos__add(struct dsos *dsos, struct dso *dso)
{
     list_add_tail(&dso->node, &dsos->head);
     __dso__findlink_by_longname(&dsos->root, dso, NULL);
     /*
      * It is now in the linked list, grab a reference, then garbage collect
      * this when needing memory, by looking at LRU dso instances in the
      * list with atomic_read(&dso->refcnt) == 1, i.e. no references
      * anywhere besides the one for the list, do, under a lock for the
      * list: remove it from the list, then a dso__put(), that probably will
      * be the last and will then call dso__delete(), end of life.
      *
      * That, or at the end of the 'struct machine' lifetime, when all
      * 'struct dso' instances will be removed from the list, in
      * dsos__exit(), if they have no other reference from some other data
      * structure.
      *
      * E.g.: after processing a 'perf.data' file and storing references
      * to objects instantiated while processing events, we will have
      * references to the 'thread', 'map', 'dso' structs all from 'struct
      * hist_entry' instances, but we may not need anything not referenced,
      * so we might as well call machines__exit()/machines__delete() and
      * garbage collect it.
      */
     dso__get(dso);
}

void dsos__add(struct dsos *dsos, struct dso *dso)
{
     down_write(&dsos->lock);
     __dsos__add(dsos, dso);
     up_write(&dsos->lock);
}

struct dso *__dsos__find(struct dsos *dsos, const char *name, bool cmp_short)
{
     struct dso *pos;

     if (cmp_short) {
          list_for_each_entry(pos, &dsos->head, node)
               if (strcmp(pos->short_name, name) == 0)
                    return pos;
          return NULL;
     }
     return __dso__find_by_longname(&dsos->root, name);
}

struct dso *dsos__find(struct dsos *dsos, const char *name, bool cmp_short)
{
     struct dso *dso;
     down_read(&dsos->lock);
     dso = __dsos__find(dsos, name, cmp_short);
     up_read(&dsos->lock);
     return dso;
}

struct dso *__dsos__addnew(struct dsos *dsos, const char *name)
{
     struct dso *dso = dso__new(name);

     if (dso != NULL) {
          __dsos__add(dsos, dso);
          dso__set_basename(dso);
          /* Put dso here because __dsos_add already got it */
          dso__put(dso);
     }
     return dso;
}

static struct dso *__dsos__findnew(struct dsos *dsos, const char *name)
{
     struct dso *dso = __dsos__find(dsos, name, false);

     return dso ? dso : __dsos__addnew(dsos, name);
}

struct dso *dsos__findnew(struct dsos *dsos, const char *name)
{
     struct dso *dso;
     down_write(&dsos->lock);
     dso = dso__get(__dsos__findnew(dsos, name));
     up_write(&dsos->lock);
     return dso;
}
