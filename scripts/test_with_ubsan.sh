#!/bin/bash
set -e

cd "$(dirname "$0")/.."

echo "=== Building with UndefinedBehaviorSanitizer ==="
bazel build --config=ubsan //:shmipc

echo "=== Running tests with UndefinedBehaviorSanitizer ==="
bazel test --cache_test_results=no --config=ubsan //core/tests:all

echo "=== UndefinedBehaviorSanitizer test complete ==="
