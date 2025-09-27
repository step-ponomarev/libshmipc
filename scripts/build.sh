#!/bin/bash
set -e

cd "$(dirname "$0")/.."
rm -rf build

cmake -S . -B build
cmake --build build -j
ctest --test-dir build --timeout 10
