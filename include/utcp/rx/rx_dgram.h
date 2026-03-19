/**
 * Logic for the server/client to receive a UDP datagram
 */

#ifndef RX_DGRAM_H
#define RX_DGRAM_H

#include <sys/types.h>


/**
 * @brief Receive a datagram and update TCB values as needed.
 * 
 * This is the all-encompassing function to receive a UDP datagram. The function
 * receives a datagram, parses it, and calls the necessary function that will
 * decide what to do with it.
 * 
 * @param udp_fd The receiving UDP socket's FD.
 * 
 */
ssize_t rcv_dgram(int udp_fd);
 
#endif