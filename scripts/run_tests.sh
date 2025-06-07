#!/bin/bash
set -e

"./$(dirname "$0")/build.sh"
ctest --test-dir build --output-on-failure
