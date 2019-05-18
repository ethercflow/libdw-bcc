#ifndef __UTILITY_H__
#define __UTILITY_H__

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#define CACHE_LINE_SIZE 64
#define cache_aligned(exp)				\
	exp __attribute__ ((aligned (CACHE_LINE_SIZE)))

#define alignof(x) __alignof__(x)

#define print_size(x) printf(" sizeof " #x": %lu\n", sizeof(x))
#define print_align(x) printf("alignof " #x": %lu\n\n", alignof(x))

void *xmalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);

#define swap(x, y) ({ typeof(x) __tmp = (x); (x) = (y); (y) = __tmp; })

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define debug(args...) do { printf(args); fflush(stdout); } while (0)

#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type, member) );})

#endif // __UTILITY_H__
