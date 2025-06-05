#include "read_write_lock.h"
#include "lock_erno.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

static const RwLock UNLOCKED = 0;
static const RwLock WAIT_READERS_FINISHED = 1;
static const RwLock LOCKED = 2;

ReadWriteLock read_write_lock_create() {
  return (ReadWriteLock){.lock = UNLOCKED, .readers = 0};
}

static bool _is_unlocked(const RwLock *);

bool read_lock(ReadWriteLock *lock) {
  if (lock == NULL) {
    lock_erno = LOCK_ERNO_INVALID_ARGUMENT;
    return false;
  }

  while (true) {
    while (!_is_unlocked(&lock->lock))
      ; // TODO: cpu_relax()

    atomic_fetch_add_explicit(&lock->readers, 1, memory_order_acquire);

    if (_is_unlocked(&lock->lock))
      return true;

    atomic_fetch_sub_explicit(&lock->readers, 1, memory_order_release);
  }
}

bool read_try_lock(ReadWriteLock *lock) {
  if (lock == NULL) {
    lock_erno = LOCK_ERNO_INVALID_ARGUMENT;
    return false;
  }

  if (!_is_unlocked(&lock->lock)) {
    lock_erno = LOCK_ERNO_OK;
    return false;
  }

  atomic_fetch_add_explicit(&lock->readers, 1, memory_order_relaxed);

  if (!_is_unlocked(&lock->lock)) {
    atomic_fetch_sub_explicit(&lock->readers, 1, memory_order_relaxed);
    lock_erno = LOCK_ERNO_OK;
    return false;
  }

  return true;
}

bool read_unlock(ReadWriteLock *lock) {
  if (lock == NULL) {
    lock_erno = LOCK_ERNO_INVALID_ARGUMENT;
    return false;
  }

  if (atomic_fetch_sub_explicit(&lock->readers, 1, memory_order_relaxed) ==
      0) { // rollback if zero dec
    atomic_fetch_add_explicit(&lock->readers, 1, memory_order_relaxed);
    lock_erno = LOCK_ERNO_NOT_HELD;
    return false;
  }

  return true;
}

bool write_lock(ReadWriteLock *lock) {
  if (lock == NULL) {
    lock_erno = LOCK_ERNO_INVALID_ARGUMENT;
    return false;
  }

  uint8_t tmp;
  // TODO: CPU relax
  do {
    tmp = atomic_load_explicit(&lock->lock, memory_order_relaxed);
  } while (tmp != UNLOCKED || !atomic_compare_exchange_strong_explicit(
                                  &lock->lock, &tmp, WAIT_READERS_FINISHED,
                                  memory_order_relaxed, memory_order_relaxed));

  // TODO: CPU relax
  while (atomic_load_explicit(&lock->readers, memory_order_relaxed) != 0)
    ;

  atomic_store_explicit(&lock->lock, LOCKED, memory_order_relaxed);

  return true;
}

bool write_unlock(ReadWriteLock *lock) {
  if (lock == NULL) {
    lock_erno = LOCK_ERNO_INVALID_ARGUMENT;
    return false;
  }

  static uint8_t expected = LOCKED;
  if (!atomic_compare_exchange_strong_explicit(&lock->lock, &expected, UNLOCKED,
                                               memory_order_relaxed,
                                               memory_order_relaxed)) {
    lock_erno = LOCK_ERNO_NOT_HELD;
    return false;
  }

  return true;
}

static bool _is_unlocked(const RwLock *lock) {
  return atomic_load_explicit(lock, memory_order_relaxed) == UNLOCKED;
}
