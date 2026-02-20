/**
 * Contains functions to transmit a datagram
 */
#ifndef TX_DGRAM_H
#define TX_DGRAM_H

#include <netinet/tcp.h>
#include <tcp/tcb.h>
#include <stdio.h>

/**
 * @brief send a buffer of data to a socket.
 * 
 * @param sock UDP sock fd to send the datagram out of
 * @param *tcb The sender's TCB
 * @param *buf data payload to append after the TCP header
 * @param len number of bytes in data payload
 * @param flags TCP header flags to append to the TCP header
 * 
 * @return `bytes_sent` the number of bytes sent in the
 * datagram, or `-1` if fails
 */
int send_dgram(int sock, tcb_t *tcb, void *buf, size_t payload_len, int flags);

/**
 * Used for ACKing a packet
 * 
 * @param *tcb The TCB of whomever is sending the ACK.
 * @param udp_sock The UDP socket of whomever is sending the ACK.
 * @param *hdr The header of the packet that is is being ACKed.
 * @param payload_len The number of bytes that make up the payload in `*hdr`.
 */
int send_ack(tcb_t *tcb, int udp_sock, struct tcphdr *hdr, ssize_t payload_len);

#endif