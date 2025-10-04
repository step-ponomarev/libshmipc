#!/bin/bash

set -e

echo "🧪 Running basic test suite..."
echo "==============================="

cd "$(dirname "$0")/.."

if [ ! -d "build" ]; then
    ./scripts/build.sh
else
    cd build
    make
    cd ..
fi

echo ""
echo "✅ Running basic tests (performance and stress tests excluded)..."
cd build
ctest --output-on-failure
cd ..

echo ""
echo "ℹ️  Note: For full testing including performance and stress tests, use:"
echo "   ./scripts/run_release_tests.sh"
