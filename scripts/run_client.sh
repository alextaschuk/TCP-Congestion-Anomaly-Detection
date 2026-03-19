#!/bin/bash

# script should be run from /build directory via `bash ../scripts/run_client.sh`

rm ../test_rcvd.txt

bash ../scripts/reset_logs.sh

time ./client_app

sha384sum ../test_rcvd.txt

scp -i /Users/alex/Downloads/mac-laptop.pem dmrocks@40.82.162.155:/home/dmrocks/alex/cosc-448-directed-study/build/log/events.csv /Users/alex/Desktop/reno_plotter
