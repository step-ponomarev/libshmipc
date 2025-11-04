#!/usr/bin/env bash
set -euo pipefail

IFS=$'\n\t'

if [[ -z "${BASH_VERSION:-}" ]]; then exec bash "$0" "$@"; fi

cd "$(dirname "${BASH_SOURCE[0]}")/.."

command -v bazel >/dev/null 2>&1 || { echo "bazel not found" >&2; exit 1; }
declare -a BAZEL_FLAGS=()
if bazel help startup_options 2>&1 | grep -q -- '--enable_bzlmod'; then
  BAZEL_FLAGS+=(--enable_bzlmod)
fi

declare -a BUILD_FLAGS=(-c opt)
declare -a TEST_FLAGS=(--test_output=errors --jobs=1 --cache_test_results=no)
bazel_cmd() {
  local subcmd=$1; shift
  if ((${#BAZEL_FLAGS[@]})); then
    bazel "${BAZEL_FLAGS[@]}" "$subcmd" "$@"
  else
    bazel "$subcmd" "$@"
  fi
}

echo "=== Build (Bazel) ==="
bazel_cmd build "${BUILD_FLAGS[@]}" //:shmipc //:shmipc_shared

echo "=== Test (Bazel, single-threaded, no cache) ==="
bazel_cmd test "${TEST_FLAGS[@]}" //core/tests:all

echo "=== Extract version from MODULE.bazel ==="
VER=$(awk -F'"' '/^[[:space:]]*version[[:space:]]*=/ {print $2; exit}' MODULE.bazel || true)
[[ -n "$VER" ]] || { echo "Failed to extract version from MODULE.bazel" >&2; exit 1; }

OS=$(uname | tr '[:upper:]' '[:lower:]')
ARCH=$(uname -m)
case "$ARCH" in
  arm64|aarch64) ARCH=arm64 ;;
  x86_64|amd64)  ARCH=x86_64 ;;
  *) echo "Unknown arch: $ARCH" >&2; exit 1 ;;
esac
PKG="libshmipc-${VER}-${OS}-${ARCH}"

BIN_DIR="$(bazel_cmd info bazel-bin)"

echo "=== Package to dist/${PKG}.tar.gz ==="
rm -rf dist && mkdir -p "dist/${PKG}/include" "dist/${PKG}/lib"
cp -R core/include/shmipc "dist/${PKG}/include/"
cp "${BIN_DIR}/core/libshmipc.a" "dist/${PKG}/lib/"

if [[ "$OS" == "darwin" ]]; then
  cp "${BIN_DIR}/core/libshmipc_shared.dylib" "dist/${PKG}/lib/libshmipc.dylib"
else
  shopt -s nullglob
  so=( "${BIN_DIR}/core"/libshmipc_shared*.so* )
  if ((${#so[@]})); then
    cp "${so[@]}" "dist/${PKG}/lib/"
  else
    echo "WARNING: libshmipc_shared.so not found in ${BIN_DIR}/core" >&2
  fi
fi

(
  cd dist
  tar -czf "${PKG}.tar.gz" "${PKG}"
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "${PKG}.tar.gz" > "${PKG}.tar.gz.sha256"
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "${PKG}.tar.gz" > "${PKG}.tar.gz.sha256"
  else
    echo "No sha256 tool found; skipping checksum" >&2
  fi
  rm -rf "${PKG}"
)

echo "=== Done ==="
ls -lah dist
