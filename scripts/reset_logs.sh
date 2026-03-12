#!/bin/bash

git pull

rm -rf ./* 2>/dev/null || true # wipe the build directory

cmake ..

make
