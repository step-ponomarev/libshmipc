#pragma once

#include <shmipc/ipc_error.h>
#include <shmipc/ipc_status.h>

static void ipc_error_init(ipc_error_t *err) {
    if (err == NULL) {
        return;
    }

    err->kind = IPC_ERR_KIND_NONE;
    err->code = IPC_ERR_CODE_NONE;
    err->message = NULL;
}

static ipc_status_t ipc_error_arg(
    ipc_error_t *err,
    ipc_error_code_t code,
    const char *message
) {
    if (err == NULL) {
        return IPC_STATUS_ERROR;
    }

    err->kind = IPC_ERR_KIND_ARG;
    err->code = code;
    err->message = message;

    return IPC_STATUS_ERROR;
}

static ipc_status_t ipc_error_arg_size(
    ipc_error_t *err,
    ipc_error_code_t code,
    ipc_error_size_t error_size,
    const char *message
) {
    if (err == NULL) {
        return IPC_STATUS_ERROR;
    }

    err->kind = IPC_ERR_KIND_ARG;
    err->code = code;
    err->message = message;
    err->as.arg.size = error_size;

    return IPC_STATUS_ERROR;
}

static ipc_status_t ipc_error_arg_capacity(
    ipc_error_t *err,
    ipc_error_capacity_t error_capacity,
    const char *message
) {
    if (err == NULL) {
        return IPC_STATUS_ERROR;
    }

    err->kind = IPC_ERR_KIND_ARG;
    err->code = IPC_ERR_CODE_INVALID_CAPACITY;
    err->message = message;
    err->as.arg.capacity = error_capacity;

    return IPC_STATUS_ERROR;
}

static ipc_status_t ipc_error_sys(
    ipc_error_t *err,
    ipc_error_code_t code,
    const char *message,
    int sys_errno
) {
    if (err == NULL) {
        return IPC_STATUS_ERROR;
    }

    err->kind = IPC_ERR_KIND_SYS;
    err->code = code;
    err->message = message;
    err->as.sys.sys_errno = sys_errno;

    return IPC_STATUS_ERROR;
}

static ipc_status_t ipc_error_internal_cas(
    ipc_error_t *err,
    ipc_error_code_t code,
    const char *message,
    ipc_error_cas_t cas_error
) {
    if (err == NULL) {
        return IPC_STATUS_ERROR;
    }

    err->kind = IPC_ERR_KIND_INTERNAL;
    err->code = code;
    err->message = message;
    err->as.internal.cas = cas_error;

    return IPC_STATUS_ERROR;
}

static ipc_status_t ipc_error_internal_corrupted(
    ipc_error_t *err,
    const char *message,
    uint64_t offset
) {
    if (err == NULL) {
        return IPC_STATUS_ERROR;
    }

    err->kind = IPC_ERR_KIND_INTERNAL;
    err->code = IPC_ERR_CODE_ENTRY_CORRUPTED;
    err->message = message;
    err->as.internal.offset = offset;

    return IPC_STATUS_ERROR;
}
