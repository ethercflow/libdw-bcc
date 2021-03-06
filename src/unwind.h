#ifndef __UNWIND_H
#define __UNWIND_H

#include "types.h"
#include "ptrace.h"

struct map;
struct symbol;
struct thread;
struct unwind_ctx;
struct stacktrace;

struct unwind_entry {
	struct map	*map;
	struct symbol	*sym;
	u64		ip;
};

typedef int (*unwind_entry_cb_t)(struct unwind_entry *entry, void *arg);

struct unwind_libunwind_ops {
	int (*prepare_access)(struct thread *thread);
	void (*flush_access)(struct thread *thread);
	void (*finish_access)(struct thread *thread);
	int (*get_entries)(unwind_entry_cb_t cb, void *arg,
					   struct thread *thread,
					   struct unwind_ctx *data,
					   struct stacktrace *st);
};

int unwind__get_entries(unwind_entry_cb_t cb, void *arg,
					   struct thread *thread,
					   struct unwind_ctx *data,
					   struct stacktrace *st);

#ifndef LIBUNWIND__ARCH_REG_SP
# define LIBUNWIND__ARCH_REG_SP    X86_SP
#endif

#ifndef LIBUNWIND__ARCH_REG_IP
# define LIBUNWIND__ARCH_REG_IP    X86_IP
#endif

int LIBUNWIND__ARCH_REG_ID(int regnum);
int unwind__prepare_access(struct thread *thread, struct map *map,
						  bool *initialized);
void unwind__flush_access(struct thread *thread);
void unwind__finish_access(struct thread *thread);

#endif /* _UNWIND_H */
