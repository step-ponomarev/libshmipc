#!/bin/bash

set -e

cd "$(dirname "$0")/.."

echo "=== Building with AddressSanitizer ==="
bazel build --config=asan //:shmipc

echo "=== Running tests with AddressSanitizer ==="
bazel test --config=asan //tests:all

echo "=== AddressSanitizer test complete ==="

