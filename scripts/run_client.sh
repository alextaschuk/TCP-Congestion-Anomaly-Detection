#!/bin/bash

# script should be run from /build directory via `bash ../scripts/run_client.sh`

rm log/events.csv
rm log/client.log

bash ../scripts/compile.sh

time ./client_app

sha384sum ../test_file.txt

#scp -i /Users/alex/Downloads/mac-laptop.pem dmrocks@40.82.162.155:/home/dmrocks/alex/cosc-448-directed-study/build/log/events.csv /Users/alex/Desktop/reno_plotter
