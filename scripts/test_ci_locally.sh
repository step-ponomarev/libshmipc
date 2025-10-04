#!/bin/bash

set -e

echo "🧪 Testing CI workflow locally..."
echo "================================="

cd "$(dirname "$0")/.."

# Clean build directory to simulate fresh CI environment
echo "🧹 Cleaning build directory..."
rm -rf build

# Install dependencies (simulate CI setup)
echo "📦 Installing dependencies..."
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "Detected Linux - would install: sudo apt-get update && sudo apt-get install -y ninja-build cmake"
    # For actual CI testing, uncomment the next line:
    # sudo apt-get update && sudo apt-get install -y ninja-build cmake
elif [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Detected macOS - would install: brew install ninja cmake"
    # For actual CI testing, uncomment the next line:
    # brew install ninja cmake
fi

# Configure (simulate CI configure step)
echo "⚙️  Configuring build..."
cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DSHMIPC_BUILD_STATIC=ON \
    -DSHMIPC_BUILD_SHARED=ON \
    -DSHMIPC_BUILD_TESTS=ON

# Build (simulate CI build step)
echo "🔨 Building..."
cmake --build build -j

# Test basic tests (simulate CI test step)
echo "✅ Running basic tests..."
ctest --test-dir build --output-on-failure -j1 --timeout 100

# Test performance and stress tests (simulate release CI)
echo "⚡ Building performance and stress tests..."
cd build

g++ -std=c++20 -O0 -g -UNDEBUG -Wall -Wextra -Wpedantic -Werror \
    -I../include -I../src -I../build/_deps/doctest-src \
    -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 \
    ../tests/ipc_channel_performance_test.cpp \
    -Lsrc -lshmipc_static -lpthread \
    -o ipc_channel_performance_test

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

echo "🚀 Running performance and stress tests..."
./ipc_channel_performance_test
./ipc_buffer_stress_test
./ipc_channel_stress_test

cd ..

# Package (simulate CI package step)
echo "📦 Creating package..."
cmake --build build --target package

echo ""
echo "🎉 CI simulation completed successfully!"
echo "✅ Your code would pass CI checks!"
