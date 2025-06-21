#ifndef IPC_UTILS_H
#define IPC_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <shmipc/ipc_common.h>
#include <time.h>

#define ALIGN_UP(s, align) (((s) + ((align) - 1)) & ~((align) - 1))
#define RELATIVE(a, max) ((a) & (max - 1))

IpcTransaction ipc_create_transaction(const uint64_t, const IpcStatus);
uint64_t ipc_timespec_to_nanos(const struct timespec *);

#ifdef __cplusplus
}
#endif

#endif
