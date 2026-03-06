#include <utils/err.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h> // for calling close() on a socket

#include <utcp/api/globals.h>
#include <utils/logger.h>

void err_sys(const char* x)
{
    const char *backup = "(null) error occurred somewhere";
    const char *msg = x ? x : backup;
    
    LOG_FATAL(msg);
    perror(msg);
    exit(EXIT_FAILURE);
}

void err_sock(int sock, const char* x)
{
    if (close(sock) == -1)
        err_sys("[err_sock] socket close failed:");

    const char *backup = "[null] socket-related error occurred somewhere";
    const char *msg = x ? x : backup;

    err_sys(msg);
}

void err_data(const char* x)
{
    const char *backup = "[null] data-related error occurred somewhere";
    const char *msg = x ? x : backup;

    fprintf(stderr, "%s\n", msg);
    exit(EXIT_FAILURE);
}