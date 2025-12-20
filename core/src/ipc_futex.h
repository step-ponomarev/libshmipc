#ifdef __APPLE__
#include <stdatomic.h>
#include <sys/errno.h>

extern int __ulock_wait(uint32_t operation, void *addr, uint64_t value,
                        uint32_t timeout);
extern int __ulock_wake(uint32_t operation, void *addr, uint64_t wake_value);

#define UL_COMPARE_AND_WAIT 1
#define ULF_WAKE_ALL 0x00000100

// timeout to timespec, common style
static inline int atomic_wait(_Atomic uint32_t *addr, uint32_t expected,
                              uint32_t timeout_ms) {
  return __ulock_wait(UL_COMPARE_AND_WAIT, addr, expected, timeout_ms * 1000);
}

static inline int atomic_wake_one(_Atomic uint32_t *addr) {
  int res = __ulock_wake(UL_COMPARE_AND_WAIT, addr,
                         0); // TODO: можно упростить в одну функцию
  if (res == -1 && errno == ENOENT) {
    return 0;
  }

  return res;
}

static inline int atomic_wake_all(_Atomic uint32_t *addr) {
  int res = __ulock_wake(UL_COMPARE_AND_WAIT | ULF_WAKE_ALL, addr, 0);
  if (res == -1 && errno == ENOENT) {
    return 0;
  }

  return res;
}

#elif __linux__
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

static inline int atomic_wait(_Atomic uint32_t *addr, uint32_t expected,
                              uint32_t timeout_ms) {
  struct timespec ts;
  ts.tv_nsec = timeout_ms * 1000000;

  return syscall(SYS_futex, addr, FUTEX_WAIT, expected, &ts);
}

static inline int atomic_wake_one(_Atomic uint32_t *addr) {
  return syscall(SYS_futex, addr, FUTEX_WAKE, 1);
}

static inline int atomic_wake_all(_Atomic uint32_t *addr) {
  return syscall(SYS_futex, addr, FUTEX_WAKE, INT_MAX);
}
#endif
