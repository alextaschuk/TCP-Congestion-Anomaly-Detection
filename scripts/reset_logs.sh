#!/bin/bash

git pull

cd build/log
rm -rf ./* 2>/dev/null || true # wipe the build directory

cmake ..

make
