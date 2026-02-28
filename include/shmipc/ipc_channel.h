#pragma once

#include <shmipc/ipc_error.h>
#include <shmipc/ipc_status.h>
#include <shmipc/ipc_export.h>
#include <time.h>

SHMIPC_BEGIN_DECLS

typedef struct ipc_channel_t ipc_channel_t;

SHMIPC_API uint64_t ipc_channel_memory_overhead(void);

SHMIPC_API uint64_t ipc_channel_min_size(void);

SHMIPC_API uint64_t ipc_channel_suggest_size(size_t desired_capacity);

SHMIPC_API uint32_t ipc_channel_get_notify_signal(ipc_channel_t *channel);

SHMIPC_API bool ipc_channel_is_retry_status(ipc_status_t status);

SHMIPC_API ipc_status_t ipc_channel_init(void *mem, size_t size, ipc_channel_t **out, ipc_error_t *err);

SHMIPC_API ipc_status_t ipc_channel_attach(void *mem, ipc_channel_t **out, ipc_error_t *err);

SHMIPC_API ipc_status_t ipc_channel_detach(ipc_channel_t *channel, ipc_error_t *err);

SHMIPC_API ipc_status_t ipc_channel_write(ipc_channel_t *channel, const void *data, size_t size, ipc_error_t *err);

SHMIPC_API ipc_status_t ipc_channel_read(ipc_channel_t *channel, ipc_entry_t *dest, const struct timespec *timeout,
                                         ipc_error_t *err);

SHMIPC_API ipc_status_t ipc_channel_try_read(ipc_channel_t *channel,
                                             ipc_entry_t *dest,
                                             ipc_error_t *err);

SHMIPC_END_DECLS
