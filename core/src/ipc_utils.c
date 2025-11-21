#include <stdint.h>
#include <time.h>

#define NANOS_PER_SEC 1000000000ULL

uint64_t ipc_timespec_to_nanos(const struct timespec *secs) {
  if (secs == NULL) {
    return 0;
  }

  return (uint64_t)secs->tv_sec * NANOS_PER_SEC + secs->tv_nsec;
}
