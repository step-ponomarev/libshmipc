#pragma once

#if defined(__clang__) || defined(__GNUC__)
#define SHMIPC_API __attribute__((visibility("default")))
#else
#define SHMIPC_API
#endif

#ifdef __cplusplus
#define SHMIPC_BEGIN_DECLS extern "C" {
#define SHMIPC_END_DECLS }
#else
#define SHMIPC_BEGIN_DECLS
#define SHMIPC_END_DECLS
#endif
