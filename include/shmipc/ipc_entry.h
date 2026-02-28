#pragma once

#include <shmipc/ipc_export.h>
#include <stdint.h>

SHMIPC_BEGIN_DECLS

typedef struct {
    uint64_t offset;
    void *payload;
    size_t size;
} ipc_entry_t;

SHMIPC_END_DECLS
