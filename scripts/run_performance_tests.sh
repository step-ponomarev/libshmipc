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

# Compile stress tests
g++ -std=c++20 -O0 -g -UNDEBUG -Wall -Wextra -Wpedantic -Werror \
    -I../include -I../src -I../build/_deps/doctest-src \
    -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 \
    ../tests/ipc_buffer_stress_test.cpp \
    -Lsrc -lshmipc_static -lpthread \
    -o ipc_buffer_stress_test

g++ -std=c++20 -O0 -g -UNDEBUG -Wall -Wextra -Wpedantic -Werror \
    -I../include -I../src -I../build/_deps/doctest-src \
    -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 \
    ../tests/ipc_channel_stress_test.cpp \
    -Lsrc -lshmipc_static -lpthread \
    -o ipc_channel_stress_test

cd ..

echo "Channel Performance Test:"
./build/ipc_channel_performance_test

if [ -z "$CI" ]; then
    echo ""
    echo "Buffer Stress Test:"
    ./build/ipc_buffer_stress_test

    echo ""
    echo "Channel Stress Test:"
    ./build/ipc_channel_stress_test
fi
