#!/bin/bash

git pull

rm log/server.log
rm log/events.csv

cmake ..
make
