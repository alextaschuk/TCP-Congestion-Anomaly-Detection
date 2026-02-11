#ifndef CONN_H
#define CONN_H

#include <stdint.h>
#include <netinet/in.h>

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
 * @brief We need to manually manage a bind() function
 * since client sockets need to be bound according to
 * their UTCP port number, not the actual UDP port number.
 * 
 * @param addr contains socket's source IP address and port number
 * 
 * @return fd on success, a "file descriptor" (index of tcb_lookup) that the
 * newly created tcb struct lives at.
 */
int bind_UTCP_sock(struct sockaddr_in *addr);

/**
 * @brief connect a UTCP socket to a remote UTCP socket (i.e., update the
 * caller's TCB struct with the remote socket's info)
 * 
 * @param utcp_fd the index of the local UTCP socket in tcb_lookup
 * @param addr contains the destination UTCP port number and IP address
 * @param dest_udp the destination UDP address
 */
void connect_utcp(int utcp_fd, struct sockaddr_in* addr, uint16_t dest_udp);

#endif