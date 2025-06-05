#ifndef IPC_READ_WRITE_LOCK_H
#define IPC_READ_WRITE_LOCK_H

#include "lock_erno.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct ReadWriteLock {
  pthread_rwlock_t lock;
} ReadWriteLock;

bool rw_init(ReadWriteLock *);
bool rw_read_lock(ReadWriteLock *);
bool rw_read_try_lock(ReadWriteLock *);

bool rw_write_lock(ReadWriteLock *);
bool rw_unlock(ReadWriteLock *);

#endif
