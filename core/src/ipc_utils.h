#pragma once

#include <shmipc/ipc_common.h>
#include <shmipc/ipc_export.h>
#include <stdbool.h>
#include <time.h>

SHMIPC_BEGIN_DECLS

#define IS_ALIGNED(x, align) (((x) & ((align) - 1)) == 0)
#define ALIGN_UP(s, align) (((s) + ((align) - 1)) & ~((align) - 1))
#define RELATIVE(a, max) ((a) & (max - 1))
#define CACHE_LINE_SIZE 64
#define ALIGN_UP_BY_CACHE_LINE(x) ALIGN_UP((x), CACHE_LINE_SIZE)
#define IS_ALIGNED_BY_CACHE_LINE(x) IS_ALIGNED((x), CACHE_LINE_SIZE)

uint64_t ipc_timespec_to_nanos(const struct timespec *);
bool is_power_of_2(const uint64_t size);

SHMIPC_END_DECLS
