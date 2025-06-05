#ifndef IPC_READ_WRITE_LOCK_H
#define IPC_READ_WRITE_LOCK_H

#include "lock_erno.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

typedef _Atomic uint8_t RwLock;

typedef struct ReadWriteLock {
  _Atomic RwLock lock;
  _Atomic uint8_t readers;
} ReadWriteLock;

ReadWriteLock rw_lock_create();
bool rw_read_lock(ReadWriteLock *);
bool rw_read_try_lock(ReadWriteLock *);
bool rw_read_unlock(ReadWriteLock *);

bool rw_write_lock(ReadWriteLock *);
bool rw_write_try_lock(ReadWriteLock *);
bool rw_write_unlock(ReadWriteLock *);

#endif
