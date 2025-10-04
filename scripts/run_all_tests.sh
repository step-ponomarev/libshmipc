#!/bin/bash

set -e

cd "$(dirname "$0")/.."

if [ ! -d "build" ]; then
    ./scripts/build.sh
else
    cd build
    make
    cd ..
fi

cd build
ctest --output-on-failure
