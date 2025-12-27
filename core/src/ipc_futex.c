#include "ipc_futex.h"

#ifdef __APPLE__
#include <sys/errno.h>

extern int __ulock_wait(uint32_t operation, void *addr, uint64_t value,
                        uint32_t timeout);
extern int __ulock_wake(uint32_t operation, void *addr, uint64_t wake_value);

#define UL_COMPARE_AND_WAIT 1
#define ULF_WAKE_ALL 0x00000100

int ipc_futex_wait(_Atomic uint32_t *addr, uint32_t expected,
                   const struct timespec *timeout) {
  return __ulock_wait(UL_COMPARE_AND_WAIT, addr, expected,
                      timeout->tv_sec * 1000000 + timeout->tv_nsec / 1000);
}

int ipc_futex_wake_one(_Atomic uint32_t *addr) {
  int res = __ulock_wake(UL_COMPARE_AND_WAIT, addr, 0);
  if (res == -1 && errno == ENOENT) {
    return 0;
  }

  return res;
}

int ipc_futex_wake_all(_Atomic uint32_t *addr) {
  int res = __ulock_wake(UL_COMPARE_AND_WAIT | ULF_WAKE_ALL, addr, 0);
  if (res == -1 && errno == ENOENT) {
    return 0;
  }

  return res;
}

#elif defined(__linux__)
#include <limits.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

int ipc_futex_wait(_Atomic uint32_t *addr, uint32_t expected,
                   const struct timespec *timeout) {
  return syscall(SYS_futex, addr, FUTEX_WAIT, expected, timeout);
}

int ipc_futex_wake_one(_Atomic uint32_t *addr) {
  return syscall(SYS_futex, addr, FUTEX_WAKE, 1);
}

int ipc_futex_wake_all(_Atomic uint32_t *addr) {
  return syscall(SYS_futex, addr, FUTEX_WAKE, INT_MAX);
}
#endif
