#!/bin/bash
set -e

cd "$(dirname "$0")/.."
rm -rf build

cmake -S . -B build -DCMAKE_INSTALL_PREFIX=dist
cmake --build build
cmake --install build

ctest --test-dir build --output-on-failure
