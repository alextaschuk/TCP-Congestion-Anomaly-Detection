#ifndef HANDLE_ACK_H
#define HANDLE_ACK_H

#include <netinet/tcp.h>

#include <tcp/tcb.h>


/**
 * @brief Handles an ACK segment for a connection that is in the `ESTABLISHED` state.
 * 
 * If the received ACK is a duplicate, the receiver's `dupacks` counter is increased by 1.
 * When 3 duplicate ACKs are detected, this function calls [functions for Tahoe & RENO] to 
 * begin congestion avoidance. If the ACK is valid, `*tcb`'s values are updated accordingly.
 * 
 * @param *tcb The receiving UDP socket's FD.
 * @param *hdr The received TCP header.
 */
void handle_ack(tcb_t *tcb, struct tcphdr *hdr, ssize_t data_len);

#endif
