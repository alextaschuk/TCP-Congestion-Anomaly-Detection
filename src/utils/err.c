#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // for calling close() on a socket

void err_sys(const char* x)
{
    /**
     * @brief A global error logging function.
     * 
     * @example err_sys("bind"); -> "bind: Address already in use"
     */
    perror(x);
    exit(EXIT_FAILURE);
}

void err_sock(int sock, const char* x)
{
    /**
     * @brief Closes a problematic socket, then prints an error message.
     * 
     * If an error occurs during the closing process, that message prints first.
     */
    if (close(sock) == -1)
        err_sys("(err_sock)socket close failed:");
    err_sys(x);
}