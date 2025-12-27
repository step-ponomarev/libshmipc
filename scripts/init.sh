#!/usr/bin/env bash
set -e

cd "$(dirname "$0")/.."

rm -rf .cache/clangd
bazel run @hedron_compile_commands//:refresh_all

