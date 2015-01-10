#!/bin/sh
set -e
mkdir -p bin
cd bin
cmake -D CMAKE_BUILD_TYPE=Release ../src/
make

