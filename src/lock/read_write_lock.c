#include "read_write_lock.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// TODO:  Сделать надежнее, поддержать дестрой лока, выводить коды ошибок
// Изучить этот API
bool rw_init(ReadWriteLock *dest) {
  if (dest == NULL) {
    lock_erno = LOCK_ERNO_INVALID_ARGUMENT;
    return false;
  }

  ReadWriteLock lock;
  pthread_rwlockattr_t attr;
  if (pthread_rwlockattr_init(&attr) != 0) {
    fprintf(stderr, "rw_init: attrs init is failed\n");
    return false;
  }

  if (pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
    fprintf(stderr, "rw_init: process shared init failed\n");
    pthread_rwlockattr_destroy(&attr);
    return false;
  }
  if (pthread_rwlock_init(&lock.lock, &attr) != 0) {
    fprintf(stderr, "rw_init: rw init is failed\n");
    return false;
  }

  memcpy(dest, &lock, sizeof(ReadWriteLock));
  pthread_rwlockattr_destroy(&attr);

  return true;
}

bool rw_read_lock(ReadWriteLock *lock) {
  if (lock == NULL) {
    lock_erno = LOCK_ERNO_INVALID_ARGUMENT;
    return false;
  }

  if (pthread_rwlock_rdlock(&lock->lock) != 0) {
    fprintf(stderr, "rw_read_lock: read lock acquiring is failed\n");
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
    fprintf(stderr, "rw_read_try_lock: read lock try acquiring is failed\n");
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
    fprintf(stderr, "rw_read_unlock: unlock is failed\n");
    lock_erno = LOCK_ERNO_ERR;
    return false;
  }

  return true;
}

bool rw_write_lock(ReadWriteLock *lock) {
  if (lock == NULL) {
    fprintf(stderr, "rw_write_lock: null arguments\n");
    lock_erno = LOCK_ERNO_INVALID_ARGUMENT;
    return false;
  }

  if (pthread_rwlock_wrlock(&lock->lock) != 0) {
    fprintf(stderr, "rw_write_lock: write lock is failed\n");
    lock_erno = LOCK_ERNO_ERR;
    return false;
  }

  printf("GOT LOCK!\n");

  return true;
}
