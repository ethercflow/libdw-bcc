#ifndef __REFCOUNT_H_
#define __REFCOUNT_H_

#include "utility.h"
#include "stdatomic.h"
#include <stdbool.h>

#ifndef UINT_MAX
# define UINT_MAX    (~0U)
#endif

typedef struct refcount_struct {
     atomic_uint refs;
} refcount_t;

static inline void refcount_set(refcount_t *r, unsigned int n)
{
     atomic_store(&r->refs, n);
}

static inline unsigned int refcount_read(refcount_t *r)
{
     return atomic_load(&r->refs);
}

static inline void refcount_inc(refcount_t *r)
{
     atomic_fetch_add(&r->refs, 1);
}

static inline __refcount_check
bool refcount_dec_and_test(refcount_t *r)
{
     atomic_fetch_sub(&r->refs, 1);
     return !atomic_load(&r->refs);
}

#endif // __REFCOUNT_H_
