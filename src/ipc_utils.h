#ifndef IPC_UTILS_H
#define IPC_UTILS_H

#include <shmipc/ipc_common.h>

#define ALIGN_UP(s, align) (((s) + ((align) - 1)) & ~((align) - 1))
#define RELATIVE(a, max) ((a) & (max - 1))

IpcTransaction ipc_create_transaction(const uint64_t, const IpcStatus);

#endif
