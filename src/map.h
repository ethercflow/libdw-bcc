#ifndef __MAP_H_
#define __MAP_H_

#include "rbtree.h"
#include "list.h"
#include "types.h"
#include "rwsem.h"
#include "refcount.h"

struct dso;
struct maps;
struct thread;
struct machine;
struct mmap2_event;

struct map {
     struct rb_node rb_node;
     struct list_head node;
     u64 start;
     u64 end;
     u8 type;
     u32 priv;
     u32 prot;
     u32 flags;
     u64 pgoff;
     u64 reloc;
     u32 maj, min;
     u64 ino;
     u64 ino_generation;

     /* ip -> dso rip */
     u64 (*map_ip)(struct map *, u64);
     /* dso rip -> ip */
     u64 (*unmap_ip)(struct map *, u64);

     struct dso *dso;
     struct maps *maps;
     refcount_t refcnt;
};

static inline u64 map_ip(struct map *map, u64 ip)
{
     return ip - map->start + map->pgoff;
}

static inline u64 unmap_ip(struct map *map, u64 ip)
{
     return ip + map->start - map->pgoff;
}

struct map *map__new(struct machine *machine,
                       struct thread *thread,
                       struct mmap2_event *event);
void map__init(struct map *map,
               struct mmap2_event *event,
               struct dso *dso);
void map__delete(struct map *map);
static inline struct map *map__get(struct map *map)
{
     if (map)
          refcount_inc(&map->refcnt);
     return map;
}
void map__put(struct map *map);
struct map *map__next(struct map *map);

struct maps {
     struct rb_root entries;
     struct list_head head;
     struct rw_semaphore lock;
     struct machine *machine;
     refcount_t refcnt;
};

struct maps *maps__new(struct machine *machine);
void maps__delete(struct maps *maps);

static inline struct maps *maps__get(struct maps *maps)
{
     if (maps)
          refcount_inc(&maps->refcnt);
     return maps;
}

void maps__put(struct maps *maps);
bool maps__empty(struct maps *maps);

struct map *maps__first(struct maps *maps);
struct map *maps__find(struct maps *maps, u64 ip);
void maps__insert(struct maps *maps, struct map *map);

#endif // __MAP_H_
