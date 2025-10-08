#!/bin/bash

set -e

cd "$(dirname "$0")/.."

echo "=== Building with ThreadSanitizer ==="
rm -rf build_tsan
mkdir -p build_tsan
cd build_tsan

cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSHMIPC_ENABLE_TSAN=ON \
  -DSHMIPC_BUILD_TESTS=ON

cmake --build . -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "=== Running tests with ThreadSanitizer ==="
ctest --output-on-failure

echo ""
echo "=== ThreadSanitizer test complete ==="

