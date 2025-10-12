#!/bin/bash
# Запуск тестов с UndefinedBehaviorSanitizer для поиска UB

set -e

cd "$(dirname "$0")/.."

echo "=== Building with UndefinedBehaviorSanitizer ==="
rm -rf build_ubsan
mkdir -p build_ubsan
cd build_ubsan

cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSHMIPC_ENABLE_UBSAN=ON \
  -DSHMIPC_BUILD_TESTS=ON

cmake --build . -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "=== Running tests with UndefinedBehaviorSanitizer ==="
ctest --output-on-failure

echo ""
echo "=== UndefinedBehaviorSanitizer test complete ==="

