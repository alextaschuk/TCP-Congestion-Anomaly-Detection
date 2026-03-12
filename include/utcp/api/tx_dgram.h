/**
 * Contains functions to transmit a datagram
 */
#ifndef TX_DGRAM_H
#define TX_DGRAM_H

#include <stdio.h>

#include <netinet/tcp.h>

#include <tcp/tcb.h>

extern uint8_t tcp_outflags[];

/**
 * @brief Send a packet data out of a UDP socket.
 * 
 * @param *tcb The sender's TCB.
 * @param snder_sock The sender's UDP socket FD to send the datagram out of.
 * @param *buf The payload of data to append after the TCP header.
 * @param payload_len The size of the payload, in bytes.
 * @param flags TCP header flags to append to the TCP header.
 * 
 * @return The number of bytes sent in the datagram, or `-1` if unsuccessful.
 */
int send_dgram(tcb_t *tcb);


/**
 * Retransmits the oldest unacked packet when the retransmission timer times out, or 3 duplicate
 * ACKs are detected.
 * 
 * @param *tcb The TCB of whomever is retransmitting the data.
 */
int retransmit_data(tcb_t *tcb, uint32_t seq);


/**
 * This is a helper function for `send_dgram()`. It constructs a TCP segment and sends it off.
 */
static int send_segment(tcb_t *tcb, uint32_t seq, size_t data_len, size_t opt_len, uint8_t flags);

#endif
