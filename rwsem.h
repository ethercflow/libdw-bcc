#ifndef __RWSEM_H_
#define __RWSEM_H_

#include <pthread.h>

struct rw_semaphore {
        pthread_rwlock_t lock;
};

int init_rwsem(struct rw_semaphore *sem);
int exit_rwsem(struct rw_semaphore *sem);

int down_read(struct rw_semaphore *sem);
int up_read(struct rw_semaphore *sem);

int down_write(struct rw_semaphore *sem);
int up_write(struct rw_semaphore *sem);

#endif // __RWSEM_H_
