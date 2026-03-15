#!/bin/bash

# script should be run from project'
# root directory via `bash scripts/run_server.sh`

# generate new 1GB test file
rm test_file.txt
cat /dev/urandom | base64 | head -c 1000000000 > test_file.txt 

bash ../scripts/reset_logs.sh

time ./server_app

sha384 ../test_file.txt