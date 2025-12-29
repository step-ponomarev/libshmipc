#include "ipc_utils.h"
#include <time.h>

#define NANOS_PER_SEC 1000000000ULL

uint64_t find_next_power_of_2(uint64_t n) {
  if (n == 0) {
    return 1;
  }

  uint64_t pow = 1;
  while (pow < n) {
    pow <<= 1;
  }

  return pow;
}

uint64_t ipc_timespec_to_nanos(const struct timespec *secs) {
  if (secs == NULL) {
    return 0;
  }

  return (uint64_t)secs->tv_sec * NANOS_PER_SEC + secs->tv_nsec;
}

inline bool is_power_of_2(const uint64_t size) {
  uint64_t res = 1;
  while (res < size) {
    res <<= 1;
  }

  return res == size;
}
