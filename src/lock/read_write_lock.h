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

ReadWriteLock read_write_lock_create();
bool read_lock(ReadWriteLock *);
bool read_try_lock(ReadWriteLock *);
bool read_unlock(ReadWriteLock *);

bool write_lock(ReadWriteLock *);
bool write_try_lock(ReadWriteLock *);
bool write_unlock(ReadWriteLock *);

#endif
