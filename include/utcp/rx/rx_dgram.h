/**
 * Logic for the server/client to receive a UDP datagram
 */

#ifndef RX_DGRAM_H
#define RX_DGRAM_H

#include <sys/types.h>


/**
 * @brief Receive a datagram and update TCB values as needed.
 * 
 * @param udp_fd The receiving UDP socket's FD.
 * @param buflen The number of bytes of the datagram payload
 * 
 * @note For a UDP socket to receive a datagram, it needs to call this function. 
 */
ssize_t rcv_dgram(int udp_fd, ssize_t buflen);
 
#endif