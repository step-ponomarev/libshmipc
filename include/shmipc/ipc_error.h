#pragma once

#include <shmipc/ipc_export.h>
#include <stdint.h>

SHMIPC_BEGIN_DECLS

typedef enum {
    IPC_ERR_KIND_NONE,

    IPC_ERR_KIND_ARG,
    IPC_ERR_KIND_INTERNAL,
    IPC_ERR_KIND_SYS
} ipc_error_kind_t;

typedef enum {
    IPC_ERR_CODE_NONE,

    IPC_ERR_CODE_NULL_ARG,
    IPC_ERR_CODE_TOO_SMALL_SIZE,
    IPC_ERR_CODE_ZERO_SIZE,
    IPC_ERR_CODE_SIZE_EXCEEDS_BUFFER,
    IPC_ERR_CODE_INVALID_CAPACITY,

    IPC_ERR_CODE_OFFSET_CAS_FAILED,
    IPC_ERR_CODE_ENTRY_CORRUPTED,

    IPC_ERR_CODE_ALLOCATION
} ipc_error_code_t;

typedef enum {
    IPC_CAS_TARGET_NONE,
    IPC_CAS_TARGET_TAIL,
    IPC_CAS_TARGET_HEAD
} ipc_cas_target_t;

typedef struct {
    ipc_cas_target_t target;
    uint64_t expected_offset;
    uint64_t actual_offset;
    uint64_t desired_offset;
} ipc_error_cas_t;

typedef struct {
    size_t limit;
    size_t requested_size;
    size_t suggested_size;
} ipc_error_size_t;

typedef struct {
    size_t provided_capacity;
    size_t required_capacity;
} ipc_error_capacity_t;

typedef struct {
    ipc_error_kind_t kind;
    ipc_error_code_t code;
    const char *message;

    union {
        struct {
            union {
                ipc_error_size_t size;
                ipc_error_capacity_t capacity;
            };
        } arg;

        struct {
            int sys_errno;
        } sys;

        struct {
            union {
                ipc_error_cas_t cas;
                uint64_t offset;
            };
        } internal;
    } as;
} ipc_error_t;


SHMIPC_END_DECLS
