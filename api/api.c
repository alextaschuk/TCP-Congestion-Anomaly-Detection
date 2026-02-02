/**
 * @brief This API contains functions, variables, etc. that are used 
 * by the server and the client.
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "api.h"

const int client_port = 4567;
const char* client_ip = "127.0.0.1";
const int server_port = 7654;
const char* server_ip = "127.0.0.1";


static void err_sys(const char* x)
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


int get_sock()
{
    /**
     * @brief create and return a socket descriptor for 
     * an Internet datagram socket using UDP.
     */
    // declare socket (sock = socket file descriptor, int that refers to the socket obj in the kernel)
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1) 
        err_sock(sock, "(get_sock)failed to create socket"); //socket failed to initialize
    else 
        fprintf(stdout, "(get_sock)socket successfully created\n");
    
    // prevent "address already in use" message when trying to rerun the server
    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    return sock;
}

void get_sock_addr(struct sockaddr_in* addr, const char* ip_addr, uint16_t port){
    /**
     * @brief Creates a socket address to the destination socket
     * 
     * Binds a destination's IP and port to a `sockaddr_in` struct.
     * 
     * @param addr Struct to store destination socket's IP address
     * and port number
     * @param ip_addr The dest socket's IP address
     * @param port The dest socket's port number
     */
    // create a socket address to the destination socket
    addr->sin_family = AF_INET;     // IPv4 (internet domain)
    addr->sin_port = htons(port);   // socket's port

    // converts human-readable IPv4 addr into 32-bit format
    if (inet_pton(AF_INET, ip_addr, &addr->sin_addr) != 1) {
        err_sys("inet_pton");
    }
}


//char* rcv_datagram()
//{
//    /**
//     * @brief Receive a datagram at an open socket
//     */
//
//}

//uint32_t calc_seq()
//{
//    /**
//     * @brief
//     */
//}
//