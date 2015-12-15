#!/usr/bin/env bash

mkdir build && cd build || exit
cmake ..
make

cd ..
rm -rf build
