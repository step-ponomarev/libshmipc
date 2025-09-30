#pragma once

#include <shmipc/ipc_export.h>
#include <stdbool.h>
#include <stdint.h>

SHMIPC_BEGIN_DECLS

// TODO: documentation
//  generic result wrapper
typedef enum { RESULT_OK = 0, RESULT_ERROR = 1 } ResultStatus;

#define IPC_RESULT(NAME, T)                                                    \
  typedef struct NAME {                                                        \
    IpcStatus ipc_status;                                                      \
    union {                                                                    \
      T result;                                                                \
      char *error_detail;                                                      \
    };                                                                         \
    ResultStatus _rs;                                                          \
  } NAME;                                                                      \
                                                                               \
  static inline NAME NAME##_ok(IpcStatus ipc_status, T result) {               \
    NAME r;                                                                    \
    r.ipc_status = ipc_status;                                                 \
    r.result = result;                                                         \
    r._rs = RESULT_OK;                                                         \
    return r;                                                                  \
  }                                                                            \
  static inline NAME NAME##_error(IpcStatus ipc_status, char *error_detail) {  \
    NAME r;                                                                    \
    r.ipc_status = ipc_status;                                                 \
    r.error_detail = error_detail;                                             \
    r._rs = RESULT_ERROR;                                                      \
    return r;                                                                  \
  }                                                                            \
  static inline bool NAME##_is_ok(NAME result) {                               \
    return result._rs == RESULT_OK;                                            \
  }                                                                            \
  static inline bool NAME##_is_error(NAME result) {                            \
    return result._rs == RESULT_ERROR;                                         \
  }

/**
 * @note Values >= 0 indicate non-error statuses (e.g., empty, not ready),
 * while values < 0 represent hard errors (e.g., invalid input, memory
 * failure).
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
  IPC_ERR_ILLEGAL_STATE = -5,
  IPC_ERR_SYSTEM = -6
} IpcStatus;

typedef uint64_t IpcEntryId;

// TODO: rewrite comment
/**
 * @brief Represents the result of an IPC buffer operation that involves a
 * specific entry.
 *
 * Used as the return type for functions that need to report both a status
 * code and an associated entry identifier. This allows the caller to
 * correlate the result with a specific position in the buffer, such as when
 * reserving, reading, peeking, or skipping entries.
 *
 * @field entry_id Identifier of the entry involved in the operation.
 *                 Valid only if `status == IPC_OK`.
 * @field status Status code indicating the outcome of the operation.
 */
typedef struct IpcTransactionResult IpcTransactionResult;
IPC_RESULT(IpcTransactionResult, IpcEntryId)

typedef struct IpcStatusResult IpcStatusResult;
IPC_RESULT(IpcStatusResult, IpcStatus)

IPC_RESULT(BooleanResult, bool)
IPC_RESULT(IntegerResult, int)
IPC_RESULT(StringResult, char *)

/**
 * @brief Represents a data entry read from the IPC buffer.
 * @field payload Pointer to the payload data.
 * @field size Size of the payload in bytes.
 */
typedef struct IpcEntry {
  void *payload;
  uint64_t size;
} IpcEntry;

SHMIPC_END_DECLS
