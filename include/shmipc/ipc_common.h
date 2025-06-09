#ifndef IPC_COMMON_H
#define IPC_COMMON_H

#include <stdint.h>

typedef enum {
  IPC_OK = 0,
  IPC_EMPTY = 1,
  IPC_NO_SPACE_CONTIGUOUS = 2,
  IPC_NOT_READY = 3,
  IPC_CORRUPTED = 4,
  IPC_LOCKED = 5,
  IPC_TRANSACTION_MISS_MATCHED = 6,
  IPC_ERR_INVALID_SIZE = -1,
  IPC_ERR_ALLOCATION = -2,
  IPC_ERR_INVALID_ARGUMENT = -3,
  IPC_ERR_TOO_SMALL = -4,
  IPC_ERR = -5
} IpcStatus;

typedef uint64_t IpcTransactionId;

typedef struct IpcTransaction {
  const IpcTransactionId id;
  const IpcStatus status;
} IpcTransaction;

typedef struct IpcEntry {
  uint64_t size;
  void *payload;
} IpcEntry;

#endif
