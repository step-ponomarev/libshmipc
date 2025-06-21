#include "ipc_utils.h"
#include "shmipc/ipc_common.h"
#include <time.h>

#define NANOS_PER_SEC 1000000000ULL

IpcTransaction ipc_create_transaction(const uint64_t id,
                                      const IpcStatus status) {
  return (IpcTransaction){.entry_id = id, .status = status};
}

uint64_t ipc_timespec_to_nanos(const struct timespec *secs) {
  if (secs == NULL) {
    return 0;
  }

  return (uint64_t)secs->tv_sec * NANOS_PER_SEC + secs->tv_nsec;
}
