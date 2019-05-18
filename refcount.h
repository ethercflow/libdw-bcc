#ifndef __REFCOUNT_H_
#define __REFCOUNT_H_

#include <stdatomic.h>

typedef struct refcount_struct {
        atomic_uint refs;
} refcount_t;

#endif // __REFCOUNT_H_
