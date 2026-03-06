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

/**
 * @brief Used in `utcp_accept()` function when a client wants to initiate a 3WHS.
 * 
 * This function is identical to `bind_UTCP_sock()`, except for 3 things:
 * 
 * 1. It doesn't set the newly-allocated TCB's source UTCP port or source IP. 
 * 
 * 2. It saves the TCB's FD inside of itself (`new_tcb->fd = fd;`).
 * 
 * 3. It returns the new TCB struct instead of its FD.
 */
tcb_t *alloc_new_tcb(void);


/**
 * Searches the global lookup table for the server's listen TCB.
 * 
 * @return A pointer to the listen TCB's struct, or `NULL` if it isn't found.
 */
tcb_t *find_listen_tcb(void);

#endif