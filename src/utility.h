#ifndef __UTILITY_H__
#define __UTILITY_H__

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

#define CACHE_LINE_SIZE 64
#define cache_aligned(exp)				\
	exp __attribute__ ((aligned (CACHE_LINE_SIZE)))

#define alignof(x) __alignof__(x)

#define print_size(x) printf(" sizeof " #x": %lu\n", sizeof(x))
#define print_align(x) printf("alignof " #x": %lu\n\n", alignof(x))

void *xmalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);

#define swap(x, y) ({ typeof(x) __tmp = (x); (x) = (y); (y) = __tmp; })

#define min(x, y) ({				\
	typeof(x) _min1 = (x);			\
	typeof(y) _min2 = (y);			\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define debug(args...) do { printf(args); fflush(stdout); } while (0)

#define container_of(ptr, type, member) ({				\
			const typeof( ((type *)0)->member ) *__mptr = (ptr); \
			(type *)( (char *)__mptr - offsetof(type, member) );})

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

#ifndef __maybe_unused
# define __maybe_unused __attribute__((unused))
#endif

#ifndef __must_check
# define __must_check __attribute__((warn_unused_result))
#endif

#ifndef __refcount_check
# define __refcount_check	__must_check
#endif

#ifndef __packed
# define __packed __attribute__((packed))
#endif

#define __AC(X,Y)	(X##Y)
#define _AC(X,Y)	__AC(X,Y)

#define CONFIG_ILLEGAL_POINTER_VALUE 0xdead000000000000
#define POISON_POINTER_DELTA _AC(CONFIG_ILLEGAL_POINTER_VALUE, UL)

/*
 * These are non-NULL pointers that will result in page faults
 * under normal circumstances, used to verify that nobody uses
 * non-initialized list entries.
 */
#define LIST_POISON1  ((void *) 0x100 + POISON_POINTER_DELTA)
#define LIST_POISON2  ((void *) 0x200 + POISON_POINTER_DELTA)

extern bool singlethreaded;

#define PATH_MAX    4096

static inline unsigned long long rdclock(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

#define ALIGN(x, a)	__ALIGN_MASK(x, (typeof(x))(a)-1)
#define __ALIGN_MASK(x, mask)	(((x)+(mask))&~(mask))

int path__join(char *bf, size_t size, const char *path1, const char *path2);
bool is_regular_file(const char *file);

#endif // __UTILITY_H__
