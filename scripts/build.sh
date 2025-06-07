#!/bin/bash
set -e

cd "$(dirname "$0")/.."
rm -rf build

cmake -S . -B build
cmake --build build
