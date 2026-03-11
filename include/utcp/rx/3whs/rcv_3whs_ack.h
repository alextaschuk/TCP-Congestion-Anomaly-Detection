#ifndef RCV_3WHS_ACK_H
#define RCV_3WHS_ACK_H

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <tcp/tcb.h>


/** 
 * @brief Handles the ACK packet that completes the 3WHS.
 * 
 * @param *listen_tcb The server's listen TCB.
 * @param *hdr The received TCP header.
 * @param from Contains info about who sent the SYN.
 */
void rcv_3whs_ack(
    tcb_t *listen_tcb,
    struct tcphdr *hdr,
    struct sockaddr_in from
);

#endif
