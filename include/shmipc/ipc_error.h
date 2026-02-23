#pragma once

#include <shmipc/ipc_export.h>
#include <stdint.h>

SHMIPC_BEGIN_DECLS

typedef enum {
    IPC_ERR_KIND_NONE,

    IPC_ERR_KIND_ARG,
    IPC_ERR_KIND_PROTOCOL,
    IPC_ERR_KIND_SYS
} ipc_error_kind_t;

typedef enum {
    IPC_ERR_CODE_NONE,

    IPC_ERR_CODE_NULL_ARG,
    IPC_ERR_CODE_TOO_SMALL_SIZE,
    IPC_ERR_CODE_INVALID_CAPACITY,

    IPC_ERR_CODE_ALLOCATION
} ipc_error_code_t;

typedef struct {
    ipc_error_kind_t kind;
    ipc_error_code_t code;
    const char *message;

    union {
        struct {
            int sys_errno;
        } sys;

        struct {
            uint64_t offset;
        } protocol;
    } as;
} ipc_error_t;


SHMIPC_END_DECLS
