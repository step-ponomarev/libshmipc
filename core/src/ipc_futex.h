#pragma once

#include <stdatomic.h>
#include <sys/errno.h>
#include <time.h>
#include <unistd.h>

int ipc_futex_wait(_Atomic uint32_t *addr, uint32_t expected,
                   const struct timespec *timeout);
int ipc_futex_wake_one(_Atomic uint32_t *addr);
int ipc_futex_wake_all(_Atomic uint32_t *addr);
