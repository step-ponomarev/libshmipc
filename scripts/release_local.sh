#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."

# Ensure bazel is available
command -v bazel >/dev/null 2>&1 || { echo "bazel not found" >&2; exit 1; }

echo "=== Build (Bazel) ==="
bazel build //:shmipc //:shmipc_shared

echo "=== Test (Bazel, single-threaded) ==="
bazel test //tests:all --test_output=errors --jobs=1

echo "=== Extract version from MODULE.bazel ==="
VER=$(awk -F'"' '/version[[:space:]]*=/ {print $2; exit}' MODULE.bazel)
if [[ -z "$VER" ]]; then
  echo "Failed to extract version from MODULE.bazel" >&2
  exit 1
fi

OS=$(uname | tr '[:upper:]' '[:lower:]')
ARCH=$(uname -m)
case "$ARCH" in
  arm64|aarch64) ARCH=arm64 ;;
  x86_64|amd64) ARCH=x86_64 ;;
  *) echo "Unknown arch: $ARCH" >&2; exit 1 ;;
esac
PKG="libshmipc-${VER}-${OS}-${ARCH}"

BIN_DIR=$(readlink bazel-bin)

echo "=== Package to dist/${PKG}.tar.gz ==="
rm -rf dist && mkdir -p "dist/${PKG}/include" "dist/${PKG}/lib"
cp -R include/shmipc "dist/${PKG}/include/"
cp "$BIN_DIR/libshmipc.a" "dist/${PKG}/lib/"
if [[ "$OS" == "darwin" ]]; then
  cp "$BIN_DIR/libshmipc_shared.dylib" "dist/${PKG}/lib/libshmipc.dylib"
else
  # Linux name likely libshmipc_shared.so
  if [[ -f "$BIN_DIR/libshmipc_shared.so" ]]; then
    cp "$BIN_DIR/libshmipc_shared.so" "dist/${PKG}/lib/libshmipc.so"
  else
    # Try to find it
    find "$BIN_DIR" -maxdepth 1 -name 'libshmipc_shared*.so*' -exec cp {} "dist/${PKG}/lib/" \; || true
  fi
fi
(
  cd dist
  tar -czf "${PKG}.tar.gz" "${PKG}"
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "${PKG}.tar.gz" > "${PKG}.tar.gz.sha256"
  elif command -v sha256sum >/dev/null 2>&1; then
    sha256sum "${PKG}.tar.gz" > "${PKG}.tar.gz.sha256"
  else
    echo "No sha256 tool found; skipping checksum" >&2
  fi
  rm -rf "${PKG}"
)

echo "=== Done ==="
echo "Artifacts in ./dist:" && ls -lah dist
