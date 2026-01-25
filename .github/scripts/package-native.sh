#!/usr/bin/env bash
set -euo pipefail

USAGE="Usage: $0 <bazel_bin_dir> <platform> <ext>"

BIN_DIR="${1:?$USAGE}"
PLATFORM="${2:?$USAGE}"
EXT="${3:?$USAGE}"

mkdir -p "native_libs/native-${PLATFORM}"
cp "${BIN_DIR}/core/libshmipc_shared.${EXT}" "native_libs/native-${PLATFORM}/libshmipc_shared.${EXT}"

echo "Packaged native_libs/native-${PLATFORM}/libshmipc_shared.${EXT}"
