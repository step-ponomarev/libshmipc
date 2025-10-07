#!/bin/bash

set -e

cd "$(dirname "$0")/.."

# Build performance and stress tests separately since they're excluded from main build
if [ ! -d "build" ]; then
    ./scripts/build.sh
fi

# Build performance and stress tests manually
cd build
echo "Building performance and stress tests..."

# Compile performance test
g++ -std=c++20 -O0 -g -UNDEBUG -Wall -Wextra -Wpedantic -Werror \
    -I../include -I../src -I../build/_deps/doctest-src \
    -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 \
    ../tests/ipc_channel_performance_test.cpp \
    -Lsrc -lshmipc_static -lpthread \
    -o ipc_channel_performance_test

# Note: Stress tests are now included in main build
# Only performance tests are compiled here

cd ..

echo "Channel Performance Test:"
./build/ipc_channel_performance_test

# Note: Interprocess and stress tests are now run as part of main build
