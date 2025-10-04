#pragma once

#include <shmipc/ipc_export.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

SHMIPC_BEGIN_DECLS

typedef enum {
  IPC_OK = 0,
  IPC_EMPTY = 1,
  IPC_ALREADY_SKIPPED = 2,
  IPC_ERR_ENTRY_TOO_LARGE = -1,
  IPC_ERR_ALLOCATION = -2,
  IPC_ERR_INVALID_ARGUMENT = -3,
  IPC_ERR_TOO_SMALL = -4,
  IPC_ERR_ILLEGAL_STATE = -5,
  IPC_ERR_SYSTEM = -6,
  IPC_ERR_NO_SPACE_CONTIGUOUS = -7,
  IPC_ERR_NOT_READY = -8,
  IPC_ERR_LOCKED = -9,
  IPC_ERR_TRANSACTION_MISMATCH = -10,
  IPC_ERR_TIMEOUT = -11,
  IPC_ERR_RETRY_LIMIT = -12,
  IPC_ERR_CORRUPTED = -13,
} IpcStatus;

#define IPC_RESULT(NAME, T, E)                                                 \
  typedef struct NAME##Error {                                                 \
    const char *detail;                                                        \
    E body;                                                                    \
    bool hasBody;                                                              \
  } NAME##Error;                                                               \
  typedef struct NAME {                                                        \
    IpcStatus ipc_status;                                                      \
    union {                                                                    \
      T result;                                                                \
      NAME##Error error;                                                       \
    };                                                                         \
  } NAME;                                                                      \
  static inline NAME NAME##_ok(IpcStatus st, T result) {                       \
    NAME r;                                                                    \
    r.ipc_status = st;                                                         \
    r.result = result;                                                         \
    return r;                                                                  \
  }                                                                            \
  static inline NAME NAME##_error(IpcStatus st, const char *detail) {          \
    NAME r;                                                                    \
    r.ipc_status = st;                                                         \
    NAME##Error e;                                                             \
    e.detail = detail;                                                         \
    e.hasBody = false;                                                         \
    r.error = e;                                                               \
    return r;                                                                  \
  }                                                                            \
  static inline NAME NAME##_error_body(IpcStatus st, const char *detail,       \
                                       E body) {                               \
    NAME r;                                                                    \
    r.ipc_status = st;                                                         \
    NAME##Error e;                                                             \
    e.detail = detail;                                                         \
    e.body = body;                                                             \
    e.hasBody = true;                                                          \
    r.error = e;                                                               \
    return r;                                                                  \
  }                                                                            \
  static inline bool NAME##_is_ok(NAME v) { return v.ipc_status >= 0; }        \
  static inline bool NAME##_is_error(NAME v) { return v.ipc_status < 0; }      \
  static inline bool NAME##_is_error_has_body(NAME##Error e) {                 \
    return e.hasBody;                                                          \
  }

#define IPC_RESULT_UNIT(NAME, ERR_T)                                           \
  typedef struct NAME##Error {                                                 \
    const char *detail;                                                        \
    ERR_T body;                                                                \
    bool hasBody;                                                              \
  } NAME##Error;                                                               \
  typedef struct NAME {                                                        \
    IpcStatus ipc_status;                                                      \
    union {                                                                    \
      unsigned char _u;                                                        \
      NAME##Error error;                                                       \
    };                                                                         \
  } NAME;                                                                      \
  static inline NAME NAME##_ok(IpcStatus st) {                                 \
    NAME r;                                                                    \
    r.ipc_status = st;                                                         \
    r._u = 0;                                                                  \
    return r;                                                                  \
  }                                                                            \
  static inline NAME NAME##_error(IpcStatus st, const char *detail) {          \
    NAME r;                                                                    \
    r.ipc_status = st;                                                         \
    NAME##Error e;                                                             \
    e.detail = detail;                                                         \
    e.hasBody = false;                                                         \
    r.error = e;                                                               \
    return r;                                                                  \
  }                                                                            \
  static inline NAME NAME##_error_body(IpcStatus st, const char *detail,       \
                                       ERR_T body) {                           \
    NAME r;                                                                    \
    r.ipc_status = st;                                                         \
    NAME##Error e;                                                             \
    e.detail = detail;                                                         \
    e.body = body;                                                             \
    e.hasBody = true;                                                          \
    r.error = e;                                                               \
    return r;                                                                  \
  }                                                                            \
  static inline bool NAME##_is_ok(NAME v) { return v.ipc_status >= 0; }        \
  static inline bool NAME##_is_error(NAME v) { return v.ipc_status < 0; }      \
  static inline bool NAME##_is_error_has_body(NAME##Error e) {                 \
    return e.hasBody;                                                          \
  }

typedef uint64_t IpcEntryId;

typedef struct IpcEntry {
  IpcEntryId id;
  void *payload;
  size_t size;
} IpcEntry;

SHMIPC_END_DECLS
