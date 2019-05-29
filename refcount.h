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

/*
 * Similar to atomic_dec_and_test(), it will WARN on underflow and fail to
 * decrement when saturated at UINT_MAX.
 *
 * Provides release memory ordering, such that prior loads and stores are done
 * before, and provides a control dependency such that free() must come after.
 * See the comment on top.
 */
static inline __refcount_check
bool refcount_sub_and_test(unsigned int i, refcount_t *r)
{
     unsigned int old, new, val = atomic_load(&r->refs);

     for (;;) {
          if (unlikely(val == UINT_MAX))
               return false;

          new = val - i;
          if (new > val) {
               fprintf(stderr, "refcount_t: underflow; use-after-free.\n");
               return false;
          }

          old = atomic_compare_exchange_strong(&r->refs, &val, new);
          if (old == val)
               break;

          val = old;
     }

     return !new;
}

static inline __refcount_check
bool refcount_dec_and_test(refcount_t *r)
{
     return refcount_sub_and_test(1, r);
}

#endif // __REFCOUNT_H_
