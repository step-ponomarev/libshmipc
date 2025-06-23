#ifndef IPC_COMMON_H
#define IPC_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @note Values >= 0 indicate non-error statuses (e.g., empty, not ready),
 * while values < 0 represent hard errors (e.g., invalid input, memory failure).
 */
typedef enum {
  IPC_OK = 0,
  IPC_EMPTY = 1,
  IPC_NO_SPACE_CONTIGUOUS = 2,
  IPC_NOT_READY = 3,
  IPC_CORRUPTED = 4,
  IPC_LOCKED = 5,
  IPC_TRANSACTION_MISS_MATCHED = 6,
  IPC_REACHED_RETRY_LIMIT = 7,
  IPC_TIMEOUT = 8,
  IPC_ALREADY_SKIPED = 9,
  IPC_ERR_ENTRY_TOO_LARGE = -1,
  IPC_ERR_ALLOCATION = -2,
  IPC_ERR_INVALID_ARGUMENT = -3,
  IPC_ERR_TOO_SMALL = -4,
  IPC_ERR = -5
} IpcStatus;

typedef uint64_t IpcEntryId;

/**
 * @brief Represents the result of an IPC buffer operation that involves a
 * specific entry.
 *
 * Used as the return type for functions that need to report both a status code
 * and an associated entry identifier. This allows the caller to correlate the
 * result with a specific position in the buffer, such as when reserving,
 * reading, peeking, or skipping entries.
 *
 * @field entry_id Identifier of the entry involved in the operation.
 *                 Valid only if `status == IPC_OK`.
 * @field status Status code indicating the outcome of the operation.
 */
typedef struct IpcTransaction {
  IpcEntryId entry_id;
  IpcStatus status;
} IpcTransaction;

/**
 * @brief Represents a data entry read from the IPC buffer.
 * @field payload Pointer to the payload data.
 * @field size Size of the payload in bytes.
 */
typedef struct IpcEntry {
  void *payload;
  uint64_t size;
} IpcEntry;

#ifdef __cplusplus
}
#endif

#endif
