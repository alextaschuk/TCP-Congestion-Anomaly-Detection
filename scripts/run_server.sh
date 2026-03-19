#!/bin/bash

# script should be run from /build directory via `bash ../scripts/run_server.sh`

# generate new 1GB test file
#rm ../test_file.txt && cat /dev/urandom | base64 | head -c 1000000000 > ../test_file.txt
#echo "Regenerated 1GB test file"

bash ../scripts/compile.sh

time ./server_app

sha384sum ../test_rcvd.txt
