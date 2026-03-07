#include "ipc_futex.h"
#include <errno.h>

#ifdef __APPLE__

// TODO: use modern api
extern int __ulock_wait(uint32_t operation, void *addr, uint64_t value,
                        uint32_t timeout);
extern int __ulock_wake(uint32_t operation, void *addr, uint64_t wake_value);

static uint32_t to_ulock_timeout_us(const struct timespec *timeout);
#define UL_COMPARE_AND_WAIT 1
#define ULF_WAKE_ALL 0x00000100

int ipc_futex_wait(_Atomic uint32_t *addr, uint32_t expected,
                   const struct timespec *timeout) {
  const uint32_t timeout_us = to_ulock_timeout_us(timeout);
  if (timeout_us == 0) {
    return 0;
  }

  int res = __ulock_wait(UL_COMPARE_AND_WAIT, addr, expected, timeout_us);
  if (res != 0) {
    // ENOENT: value changed before we slept (compare failed) - this is normal
    // EINTR: interrupted by signal - continue waiting
    if (errno == ENOENT || errno == EINTR) {
      return 0; // Treat as success, continue loop
    }
    // For ETIMEDOUT and other errors, return -1

    return errno;
  }

  return res;
}

int ipc_futex_wake_one(_Atomic uint32_t *addr) {
  int res = __ulock_wake(UL_COMPARE_AND_WAIT, addr, 0);
  if (res != 0 && errno == ENOENT) {
    return 0;
  }

  return res;
}

int ipc_futex_wake_all(_Atomic uint32_t *addr) {
  int res = __ulock_wake(UL_COMPARE_AND_WAIT | ULF_WAKE_ALL, addr, 0);
  if (res != 0 && errno == ENOENT) {
    return 0;
  }

  return res;
}

static uint32_t to_ulock_timeout_us(const struct timespec *timeout) {
  if (timeout == NULL) {
    return 0;
  }

  uint64_t us = (uint64_t) timeout->tv_sec * 1000000ULL;
  us += (uint64_t) timeout->tv_nsec / 1000ULL;

  if (us == 0) {
    return 0;
  }

  if (us > UINT32_MAX) {
    us = UINT32_MAX;
  }

  return (uint32_t) us;
}

#elif defined(__linux__)
#include <limits.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

int ipc_futex_wait(_Atomic uint32_t *addr, uint32_t expected,
                   const struct timespec *timeout) {
  int res = syscall(SYS_futex, addr, FUTEX_WAIT, expected, timeout);
  if (res != 0) {
    // EAGAIN: value changed before we slept - this is normal, continue loop
    // EINTR: interrupted by signal - continue waiting
    // ETIMEDOUT: timeout expired - return error so caller can check
    if (errno == EAGAIN || errno == EINTR) {
      return 0; // Treat as success, continue loop
    }
    // For other errors (including ETIMEDOUT), return the error
    // Caller should check timeout separately

    return errno;
  }

  return res;
}

int ipc_futex_wake_one(_Atomic uint32_t *addr) {
  return syscall(SYS_futex, addr, FUTEX_WAKE, 1);
}

int ipc_futex_wake_all(_Atomic uint32_t *addr) {
  return syscall(SYS_futex, addr, FUTEX_WAKE, INT_MAX);
}
#endif
