# run inside the ./build directory
bash utils/cmds/rest_logs.sh
cmake ..
make
clear
time ./server_app