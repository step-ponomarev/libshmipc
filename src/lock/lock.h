#ifndef IPC_LOCK_H
#define IPC_LOCK_H

#include "lock_erno.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

typedef _Atomic uint8_t Lock;

Lock lock_create();
bool lock_lock(Lock *);
bool lock_try_lock(Lock *);
bool lock_unlock(Lock *);

#endif
