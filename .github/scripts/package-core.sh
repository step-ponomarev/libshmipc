#!/usr/bin/env bash
set -euo pipefail

USAGE="Usage: $0 <version> <os> <arch> <bazel_bin_dir>"

VERSION="${1:?$USAGE}"
OS="${2:?$USAGE}"
ARCH="${3:?$USAGE}"
BIN_DIR="${4:?$USAGE}"

PKG="libshmipc-${VERSION}-${OS}-${ARCH}"

mkdir -p "dist/${PKG}/include" "dist/${PKG}/lib"
cp -R include/shmipc "dist/${PKG}/include/"
cp "${BIN_DIR}/core/libshmipc.a" "dist/${PKG}/lib/"

if [[ "${OS}" == "macos" ]]; then
  cp "${BIN_DIR}/core/libshmipc_shared.dylib" "dist/${PKG}/lib/libshmipc.dylib"
else
  if [[ -f "${BIN_DIR}/core/libshmipc_shared.so" ]]; then
    cp "${BIN_DIR}/core/libshmipc_shared.so" "dist/${PKG}/lib/libshmipc.so"
  else
    find "${BIN_DIR}/core" -maxdepth 1 -name 'libshmipc_shared*.so*' -exec cp {} "dist/${PKG}/lib/" \; || true
  fi
fi

(cd dist && tar -czf "${PKG}.tar.gz" "${PKG}")
(cd dist && shasum -a 256 "${PKG}.tar.gz" >"${PKG}.tar.gz.sha256")
rm -rf "dist/${PKG}"

echo "Created dist/${PKG}.tar.gz"
