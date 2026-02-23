#pragma once

#include <shmipc/ipc_export.h>

SHMIPC_BEGIN_DECLS

typedef enum {
    IPC_STATUS_OK,
    IPC_STATUS_BUSY,
    IPC_STATUS_NO_SPACE,
    IPC_STATUS_ERROR
} ipc_status_t;

SHMIPC_END_DECLS
