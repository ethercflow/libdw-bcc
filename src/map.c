#include "dso.h"
#include "map.h"
#include "event.h"
#include "machine.h"
#include "utility.h"
#include <assert.h>


void map__init(struct map *map, struct mmap2_event *event, struct dso *dso)
{
	map->maj = event->maj;
	map->min = event->min;
	map->ino = event->ino;
	map->ino_generation = event->ino_generation;
	map->prot = event->prot;
	map->flags = event->flags;
	map->start = event->start;
	map->end = map->start + event->len;
	map->pgoff = event->pgoff;
	map->reloc = 0;
	map->dso = dso__get(dso);
	map->map_ip = map_ip;
	map->unmap_ip = unmap_ip;
	RB_CLEAR_NODE(&map->rb_node);
	INIT_LIST_HEAD(&map->node);
	refcount_set(&map->refcnt, 1);
}

struct map *map__new(struct machine *machine,
					 struct thread *thread __maybe_unused,
					 struct mmap2_event *event)
{
	struct map *map;
	struct dso *dso;

	map = xmalloc(sizeof(*map));

	dso = machine__findnew_dso(machine, event->filename);
	assert(dso != NULL);

	map__init(map, event, dso);
	dso__put(dso);

	return map;
}

static void map__exit(struct map *map)
{
	assert(RB_EMPTY_NODE(&map->rb_node));
	dso__zput(map->dso);
}

void map__delete(struct map *map)
{
	map__exit(map);
	free(map);
}

void map__put(struct map *map)
{
	if (map && refcount_dec_and_test(&map->refcnt)) {
		list_del_init(&map->node);
		map__delete(map);
	}
}

struct map *map__next(struct map *map) {
	struct rb_node *next = rb_next(&map->rb_node);

	if (next)
		return rb_entry(next, struct map, rb_node);
	return NULL;
}

static void maps__init(struct maps *maps, struct machine *machine)
{
	maps->entries = RB_ROOT;
	INIT_LIST_HEAD(&maps->head);
	init_rwsem(&maps->lock);
	maps->machine = machine;
}

static void __maps__purge(struct maps *maps)
{
	struct rb_root *root = &maps->entries;
	struct rb_node *next = rb_first(root);

	while (next) {
		struct map *pos = rb_entry(next, struct map, rb_node);

		next = rb_next(&pos->rb_node);
		rb_erase_init(&pos->rb_node, root);
		map__put(pos);
	}
}

static void maps__exit(struct maps *maps)
{
	down_write(&maps->lock);
	__maps__purge(maps);
	up_write(&maps->lock);
}

struct maps *maps__new(struct machine *machine)
{
	struct maps *maps = xmalloc(sizeof(*maps));

	maps__init(maps, machine);
	refcount_set(&maps->refcnt, 1);

	return maps;
}

void maps__put(struct maps *maps)
{
	if (maps && refcount_dec_and_test(&maps->refcnt))
		maps__delete(maps);
}

bool maps__empty(struct maps *maps)
{
	if (maps__first(maps))
		return false;

	return true;
}

void maps__delete(struct maps *maps)
{
	maps__exit(maps);
	free(maps);
}

struct map *maps__first(struct maps *maps)
{
	struct rb_node *first = rb_first(&maps->entries);

	if (first)
		return rb_entry(first, struct map, rb_node);
	return NULL;
}

struct map *maps__find(struct maps *maps, u64 ip)
{
	struct rb_node **p, *parent = NULL;
	struct map *m;

	down_read(&maps->lock);

	p = &maps->entries.rb_node;
	while (*p != NULL) {
		parent = *p;
		m = rb_entry(parent, struct map, rb_node);
		if (ip < m->start)
			p = &(*p)->rb_left;
		else if (ip >= m->end)
			p = &(*p)->rb_right;
		else
			goto out;
	}

	m = NULL;
out:
	up_read(&maps->lock);
	return m;
}


static void __maps__insert(struct maps *maps, struct map *map)
{
	struct rb_node **p = &maps->entries.rb_node;
	struct rb_node *parent = NULL;
	const u64 ip = map->start;
	struct map *m;

	while (*p != NULL) {
		parent = *p;
		m = rb_entry(parent, struct map, rb_node);
		if (ip < m->start)
			p = &(*p)->rb_left;
		else
			p = &(*p)->rb_right;
	}

	rb_link_node(&map->rb_node, parent, p);
	rb_insert_color(&map->rb_node, &maps->entries);
	list_add_tail(&map->node, &maps->head);
	map__get(map);
}

void maps__insert(struct maps *maps, struct map *map)
{
	down_write(&maps->lock);
	__maps__insert(maps, map);
	up_write(&maps->lock);
}
