#!/bin/bash

mkdir -p build
cd build
cmake .. -DCDRC_TEST=On
cmake --build .
ctest -C Debug --no-tests=error --output-on-failure
cd -