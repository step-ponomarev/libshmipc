#!/bin/bash
set -e

cd "$(dirname "$0")/.."

echo "=== Building libshmipc with Bazel ==="
bazel build //:shmipc

echo "=== Running tests ==="
bazel test --cache_test_results=no //tests:all
