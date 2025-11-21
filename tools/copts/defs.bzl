SHMIPC_BASE_DEFINES = [
    "_POSIX_C_SOURCE=200809L",
    "_XOPEN_SOURCE=700",
]

SHMIPC_BASE_COPTS = [
    "-std=c11",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
]

SHMIPC_DBG_COPTS = [
    "-O0",
    "-g",
    "-UNDEBUG",
    "-fno-omit-frame-pointer",
]

SHMIPC_OPT_COPTS = [
    "-O3",
    "-DNDEBUG",
    "-fno-omit-frame-pointer",
]

def shmipc_copts():
    return SHMIPC_BASE_COPTS + select({
        "//tools/build:dbg": SHMIPC_DBG_COPTS,
        "//tools/build:opt": SHMIPC_OPT_COPTS,
        "//conditions:default": SHMIPC_OPT_COPTS,
    })

def shmipc_defines():
    return SHMIPC_BASE_DEFINES
