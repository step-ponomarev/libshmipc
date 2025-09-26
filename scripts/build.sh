#!/bin/bash
set -e

cd "$(dirname "$0")/.."
rm -rf build

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSHMIPC_BUILD_SHARED=ON -DSHMIPC_BUILD_STATIC=ON -DSHMIPC_BUILD_TESTS=ON -DSHMIPC_ENABLE_WERROR=ON
cmake --build build -j
ctest --test-dir build -VV -j1 --timeout 120
