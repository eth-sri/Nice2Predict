#!/bin/sh
set -e
mkdir -p bin
cd bin
cmake -D test=ON -D CMAKE_BUILD_TYPE=Debug -H../src/
make -j4
