#ifndef __LIST_H__
#define __LIST_H__

#include "utility.h"
#include <stdlib.h>

typedef struct __list_t {
	struct __list_t *next;
} list_t;

typedef struct {
	list_t *last;
} list_head_t;

#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

#define LIST_HEAD(name) list_head_t name = { .last = NULL }

static inline void list_init(list_head_t *head)
{
	head->last = NULL;
}

static inline int list_empty(const list_head_t *head)
{
	return head->last == NULL;
}

static inline list_t* list_first(const list_head_t *head)
{
	return head->last->next;
}

static inline list_t* list_last(const list_head_t *head)
{
	return head->last;
}

static inline list_t* list_next(const list_t *x)
{
	return x->next;
}

static inline void list_init_and_add(list_head_t *head, list_t *new)
{
	new->next = new;
	head->last = new;
}

static inline void list_add_first(list_head_t *head, list_t *new)
{
	if (list_empty(head)) {
		new->next = new;
		head->last = new;
	} else {
		new->next = head->last->next;
		head->last->next = new;
	}
}

static inline void list_add_last(list_head_t *head, list_t *new)
{
	list_add_first(head, new);
	head->last = new;
}

static inline list_t* list_pop_first(list_head_t *head)
{
	list_t *ret = NULL;
	if (!list_empty(head)) {
		ret = list_first(head);
		list_t *last = list_last(head);
		if (ret == last)
			list_init(head);
		else
			last->next = ret->next;
	}
	return ret;
}

static inline void list_move(list_head_t *to, const list_head_t *fr)
{
	*to = *fr;
}

static inline void list_splice_tail(list_head_t *to, list_head_t *from)
{
	if (list_empty(from))
		return;

	if (!list_empty(to))
		swap(to->last->next, from->last->next);
	*to = *from;
}

static inline void list_splice_tail_init(list_head_t *to, list_head_t *from)
{
	list_splice_tail(to, from);
	list_init(from);
}

#endif /* __LIST_H__ */
