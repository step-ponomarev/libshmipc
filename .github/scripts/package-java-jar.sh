#!/usr/bin/env bash
set -euo pipefail

USAGE="Usage: $0 <version> <base_jar> <jni_libs_dir>"

VERSION="${1:?$USAGE}"
BASE_JAR="$(realpath "${2:?$USAGE}")"
JNI_LIBS_DIR="$(realpath "${3:?$USAGE}")"

FINAL_JAR="dist/libshmipc-java-${VERSION}.jar"
EXPECTED_PLATFORMS=("Linux-x86_64" "Darwin-arm64" "Darwin-x86_64")

mkdir -p dist jar_temp
cd jar_temp

jar xf "${BASE_JAR}"

for platform_dir in "${JNI_LIBS_DIR}"/jni-*/; do
  [ -d "$platform_dir" ] || continue
  platform=$(basename "$platform_dir" | sed 's/^jni-//')

  lib_file=$(find "$platform_dir" -name "libshmipc_jni.*" -type f | head -1)
  if [ -n "$lib_file" ]; then
    mkdir -p "native/${platform}"
    cp "$lib_file" "native/${platform}/"
    echo "Added: native/${platform}/$(basename "$lib_file")"
  fi
done

for platform in "${EXPECTED_PLATFORMS[@]}"; do
  if ! ls native/"${platform}"/libshmipc_jni.* >/dev/null 2>&1; then
    echo "ERROR: Missing native library for ${platform}" >&2
    exit 1
  fi
done

jar cf "../${FINAL_JAR}" ./*
cd ..

shasum -a 256 "${FINAL_JAR}" >"${FINAL_JAR}.sha256"

echo "Created: ${FINAL_JAR}"
