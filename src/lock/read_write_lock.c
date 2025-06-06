#include "read_write_lock.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

bool rw_init(ReadWriteLock *dest) {
  if (dest == NULL) {
    lock_erno = LOCK_ERNO_INVALID_ARGUMENT;
    return false;
  }

  ReadWriteLock lock;
  if (pthread_rwlock_init(&lock.lock, NULL) != 0) {
    perror("rw_init: rw init is failed\n");
    return false;
  }

  memcpy(dest, &lock, sizeof(ReadWriteLock));

  return true;
}

bool rw_read_lock(ReadWriteLock *lock) {
  if (lock == NULL) {
    lock_erno = LOCK_ERNO_INVALID_ARGUMENT;
    return false;
  }

  if (pthread_rwlock_rdlock(&lock->lock) != 0) {
    perror("rw_read_lock: read lock acquiring is failed\n");
    lock_erno = LOCK_ERNO_ERR;
    return false;
  }

  return true;
}

bool rw_read_try_lock(ReadWriteLock *lock) {
  if (lock == NULL) {
    lock_erno = LOCK_ERNO_INVALID_ARGUMENT;
    return false;
  }

  const int res = pthread_rwlock_tryrdlock(&lock->lock);
  if (res == 0) {
    return true;
  }

  if (res != EBUSY) {
    perror("rw_read_try_lock: read lock try acquiring is failed\n");
    lock_erno = LOCK_ERNO_ERR;
  }

  return false;
}

bool rw_unlock(ReadWriteLock *lock) {
  if (lock == NULL) {
    lock_erno = LOCK_ERNO_INVALID_ARGUMENT;
    return false;
  }

  if (pthread_rwlock_unlock(&lock->lock) != 0) {
    perror("rw_read_unlock: unlock is failed\n");
    lock_erno = LOCK_ERNO_ERR;
    return false;
  }

  return true;
}

bool rw_write_lock(ReadWriteLock *lock) {
  if (lock == NULL) {
    lock_erno = LOCK_ERNO_INVALID_ARGUMENT;
    return false;
  }

  if (pthread_rwlock_wrlock(&lock->lock) != 0) {
    perror("rw_write_lock: write lock is failed\n");
    lock_erno = LOCK_ERNO_ERR;
    return false;
  }

  return true;
}
