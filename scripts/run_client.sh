#!/bin/bash

# script should be run from /build directory via `bash ../scripts/run_client.sh`

rm ../test_rcvd.txt && touch ../test_rcvd.txt

bash ../scripts/reset_logs.sh

time ./client_app

sha384 ../test_rcvd.txt