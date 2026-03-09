cmake_minimum_required(VERSION 3.10)
project(utcp C)

# compiler flags
set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra -g)

include_directories(include)

# source groups
file(GLOB_RECURSE COMMON_SRCS 
    "src/tcp/*.c"
    "src/utcp/api/*.c"
    "src/utcp/rx/*.c"
    "src/utils/*.c"
)

# 1. Bring in the zlog source
add_subdirectory(vendor/zlog EXCLUDE_FROM_ALL)

# logger setup
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/log/client)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/log/server)

# copy config files into /build dir
configure_file(zlog_client.conf ${CMAKE_CURRENT_BINARY_DIR}/zlog_client.conf COPYONLY)
configure_file(zlog_server.conf ${CMAKE_CURRENT_BINARY_DIR}/zlog_server.conf COPYONLY)

add_executable(client_app src/utcp/client.c ${COMMON_SRCS})
add_executable(server_app src/utcp/server.c ${COMMON_SRCS})
#add_executable(server_app src/app/server_application.c ${COMMON_SRCS})

# link zlog so that the compiler knows where it is
target_link_libraries(client_app PRIVATE zlog)
target_link_libraries(server_app PRIVATE zlog)

# make sure the code can find zlog's headers
target_include_directories(client_app PRIVATE vendor/zlog/src)
target_include_directories(server_app PRIVATE vendor/zlog/src)