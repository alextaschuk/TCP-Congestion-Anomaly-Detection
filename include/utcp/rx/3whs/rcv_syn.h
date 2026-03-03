#ifndef RCV_SYN_H
#define RCV_SYN_H

#include <netinet/tcp.h>

#include <tcp/tcb.h>


/**
 * @brief Handles a SYN packet and sends a SYN-ACK to the sender in response.
 * 
 * When a SYN request is received, the listen socket will attempt to create a new socket
 * and TCB for the new connection.
 * 
 * @param *listen_tcb The listen socket's TCB.
 * @param udp_fd The sender's UDP socket FD.
 * @param *hdr The TCP header of the received packet.
 * @param data_len The length of the TCP packet's payload (throws `err_data` if this is > 0).
 * @param from Contains info about who sent the SYN.
 */
void rcv_syn(
    tcb_t *listen_tcb,
    int udp_fd,
    struct tcphdr *hdr,
    ssize_t data_len,
    struct sockaddr_in from
);

#endif 