#pragma once

#include <shmipc/ipc_export.h>
#include <stdint.h>
#include <stddef.h>

SHMIPC_BEGIN_DECLS

typedef struct {
    uint64_t offset;
    void *payload;
    size_t size;
} ipc_entry_t;

SHMIPC_API void ipc_entry_destroy(ipc_entry_t entry);

SHMIPC_END_DECLS
