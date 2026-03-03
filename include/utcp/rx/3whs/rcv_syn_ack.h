#ifndef RCV_SYN_ACK_H
#define RCV_SYN_ACK_H

#include <tcp/tcb.h>
#include <netinet/tcp.h>

/**
 * Handles a SYN-ACK packet and sends an ACK to the sender in response.
 * 
 * @param *tcb The client TCB of the UTCP socket that received the packet.
 * @param *udp_fd The client's UDP socket FD.
 * @param *hdr The received TCP header.
 * @param data_len The length of the TCP packet's payload, in bytes.
 * 
 * @note `data_len` should be 0 (this check is performed in the function) because we are not
 * using TCP Fast Open.
 */
void rcv_syn_ack(
    tcb_t *tcb,
    int udp_fd,
    struct tcphdr *hdr,
    ssize_t data_len
);

#endif