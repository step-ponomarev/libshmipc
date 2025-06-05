#include "lock.h"
#include "lock_erno.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>

static const Lock UNLOCKED = 0;
static const Lock LOCKED = 1;

Lock lock_create() { return UNLOCKED; }

bool lock_lock(Lock *lock) {
  if (lock == NULL) {
    lock_erno = LOCK_ERNO_INVALID_ARGUMENT;
    return false;
  }

  uint8_t tmp;
  do {
    // TODO: release cpu core
    //
    tmp = atomic_load(lock);
  } while (tmp != UNLOCKED ||
           !atomic_compare_exchange_strong(lock, &tmp, LOCKED));

  return true;
}

bool lock_try_lock(Lock *lock) {
  if (lock == NULL) {
    lock_erno = LOCK_ERNO_INVALID_ARGUMENT;
    return false;
  }

  static uint8_t expected = UNLOCKED;
  uint8_t tmp;
  do {
    tmp = atomic_load(lock);
    if (tmp == LOCKED) {
      lock_erno = LOCK_OK;
      return false;
    }
  } while (!atomic_compare_exchange_strong(lock, &expected, LOCKED));

  return true;
}

bool lock_unlock(Lock *lock) {
  if (lock == NULL) {
    lock_erno = LOCK_ERNO_INVALID_ARGUMENT;
    return false;
  }

  static uint8_t expected = LOCKED;
  if (!atomic_compare_exchange_strong(lock, &expected, UNLOCKED)) {
    lock_erno = LOCK_ERNO_NOT_HELD;
    return false;
  }

  return true;
}
