#!/bin/bash
set -e

cd "$(dirname "$0")/.."
rm -rf build

CC=$(brew --prefix)/bin/gcc-15 CXX=/usr/bin/clang++ \
  cmake -S . -B build
cmake --build build -j
ctest --test-dir build --timeout 10
