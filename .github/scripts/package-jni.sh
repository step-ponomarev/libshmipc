#!/usr/bin/env bash
set -euo pipefail

USAGE="Usage: $0 <bazel_bin_dir> <platform> <ext>"

BIN_DIR="${1:?$USAGE}"
PLATFORM="${2:?$USAGE}"
EXT="${3:?$USAGE}"

mkdir -p "jni_libs/native/${PLATFORM}"
cp "${BIN_DIR}/java/native/libshmipc_jni.${EXT}" "jni_libs/native/${PLATFORM}/libshmipc_jni.${EXT}"

echo "Packaged jni_libs/native/${PLATFORM}/libshmipc_jni.${EXT}"
