#!/bin/bash
set -e

cd "$(dirname "$0")/.."

echo "=== Building with ThreadSanitizer ==="
bazel build --config=tsan //:shmipc

echo "=== Running tests with ThreadSanitizer ==="
bazel test --config=tsan //tests:all

echo "=== ThreadSanitizer test complete ==="

