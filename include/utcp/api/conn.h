#ifndef CONN_H
#define CONN_H
/*
Logic related to connection establishment and management,
whether that be for a UDP socket, a UTCP socket, or anything
else.
*/

#include <stdint.h>
#include <netinet/in.h>

#include <tcp/tcb.h>

/**
 * @brief create and return a socket descriptor for 
 * an Internet datagram socket using UDP.
 * 
 * @param pts (port to set), the client passes in the 
 * client_port, and the server passes in the server_port variable
 * to dynamically set the values to the port that the kernel
 * chooses. If we hardcode the port, this value is not needed.
 */
int bind_UDP_sock(int pts);

/**
 * @brief We need to manually manage a `bind()` function
 * since client sockets need to be bound according to
 * their UTCP port number, not the actual UDP port number.
 * 
 * @param addr contains socket's source IP address and port number
 * 
 * @return `int fd` on success, the UTCP socket's "file descriptor". I.e.,
 *  the newly created `tcb_t` struct is stored at `tcb_lookup[fd]`.
 */
int bind_UTCP_sock(struct sockaddr_in *addr);

tcb_t *alloc_new_tcb(void);

tcb_t *find_listen_tcb(void);

#endif