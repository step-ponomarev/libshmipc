#!/bin/bash

set -e

cd "$(dirname "$0")/.."

if [ ! -d "build" ]; then
    ./scripts/build.sh
else
    cd build
    make ipc_channel_performance_test ipc_buffer_stress_test ipc_channel_stress_test
    cd ..
fi

echo "Channel Performance Test:"
./build/tests/ipc_channel_performance_test

echo ""
echo "Buffer Stress Test:"
./build/tests/ipc_buffer_stress_test

echo ""
echo "Channel Stress Test:"
./build/tests/ipc_channel_stress_test
