#pragma once

#include <shmipc/ipc_error.h>
#include <shmipc/ipc_status.h>

static void ipc_error_init(ipc_error_t* err) {
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
