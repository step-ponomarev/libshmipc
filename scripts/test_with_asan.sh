#!/bin/bash

set -e

cd "$(dirname "$0")/.."

echo "=== Building with AddressSanitizer ==="
rm -rf build_asan
mkdir -p build_asan
cd build_asan

cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSHMIPC_ENABLE_ASAN=ON \
  -DSHMIPC_BUILD_TESTS=ON

cmake --build . -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "=== Running tests with AddressSanitizer ==="
ctest --output-on-failure

echo ""
echo "=== AddressSanitizer test complete ==="

