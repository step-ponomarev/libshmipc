#!/usr/bin/env bash
set -e

cd "$(dirname "$0")/.."

bazel run @hedron_compile_commands//:refresh_all