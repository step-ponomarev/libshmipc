#!/bin/bash
set -e

cd "$(dirname "$0")/.."
rm -rf build

CC=$(brew --prefix)/bin/gcc-15 CXX=/usr/bin/clang++ \
  cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j

cp build/compile_commands.json . 2>/dev/null || true

ln -sf build/_deps/doctest-src/doctest third_party_doctest 2>/dev/null || true

ctest --test-dir build --timeout 30 -V 