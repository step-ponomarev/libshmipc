#!/bin/bash

set -e

echo "🚀 Running FULL test suite for release..."
echo "=========================================="

cd "$(dirname "$0")/.."

# Clean build to ensure fresh state
echo "📦 Building project..."
./scripts/build.sh

# Run basic tests (already included in build.sh, but let's be explicit)
echo ""
echo "✅ Running basic tests..."
cd build
ctest --test-dir build --timeout 20 -V
cd ..

# Run performance and stress tests
echo ""
echo "⚡ Running performance and stress tests..."
./scripts/run_performance_tests.sh

echo ""
echo "🎉 All tests completed successfully!"
echo "✅ Project is ready for release!"
