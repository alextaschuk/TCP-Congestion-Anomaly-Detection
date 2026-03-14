#!/bin/bash

rm ../test_rcvd.txt
touch ../test_rcvd.txt

bash ../scripts/reset_logs.sh

time ./client_app