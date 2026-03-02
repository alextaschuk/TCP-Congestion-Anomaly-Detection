/**
 * Contains functions to transmit a datagram
 */
#ifndef TX_DGRAM_H
#define TX_DGRAM_H

#include <netinet/tcp.h>
#include <tcp/tcb.h>
#include <stdio.h>

/**
 * @brief Send a packet data out of a UDP socket.
 * 
 * @param *snder_tcb The sender's TCB.
 * @param snder_sock The sender's UDP socket FD to send the datagram out of.
 * @param *buf The payload of data to append after the TCP header.
 * @param payload_len The size of the payload, in bytes.
 * @param flags TCP header flags to append to the TCP header.
 * 
 * @return The number of bytes sent in the datagram, or `-1` if unsuccessful.
 */
int send_dgram(tcb_t *snder_tcb, int snder_sock, void *buf, size_t payload_len, int flags);

#endif
