#ifndef HANDLE_DATA_H
#define HANDLE_DATA_H

#include <netinet/tcp.h>

#include <tcp/tcb.h>


/**
 * Handles a TCP segment that contains a payload for a UTCP socket that is in the `ESTABLISHED` state.
 * 
 * @param *tcb A server TCB of the UTCP socket that is receiving the segment.
 * @param udp_fd The server's UDP socket to send data (e.g. ACK) out of.
 * @param *hdr The received TCP segment's header.
 * @param *data The received TCP segment's payload.
 * @param data_len The size of `*data`, in bytes.
 */
void handle_data(
    tcb_t *tcb,
    int udp_fd,
    struct tcphdr *hdr, 
    uint8_t *data,
    ssize_t data_len
);

#endif
