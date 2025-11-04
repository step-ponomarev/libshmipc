#pragma once

#include <shmipc/ipc_common.h>
#include <shmipc/ipc_export.h>
#include <time.h>

SHMIPC_BEGIN_DECLS

#define ALIGN_UP(s, align) (((s) + ((align) - 1)) & ~((align) - 1))
#define RELATIVE(a, max) ((a) & (max - 1))

uint64_t ipc_timespec_to_nanos(const struct timespec *);

SHMIPC_END_DECLS
