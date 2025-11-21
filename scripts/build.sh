#!/bin/bash
set -e

cd "$(dirname "$0")/.."

echo "=== Building libshmipc (core + Java) with Bazel ==="
bazel build //:build_all

echo "=== Running tests ==="
bazel test --cache_test_results=no //core/tests:all //java:all_tests
