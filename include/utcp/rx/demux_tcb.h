#ifndef DEMUX_TCB_H
#define DEMUX_TCB_H

#include <utcp/api/globals.h>

/**
 * Searches for a TCB that is either active (FSM state is `ESTABLISHED`), or a half-open connection
 * (FSM state is `SYN_RECEIVED`) using the remote sender's info and the local UTCP port.
 * 
 * @param *global A pointer to the global `api_t` instance.
 * @param dest_utcp_port The TCP header's dest UTCP port number (the receiver's UTCP port).
 * @param src_ip The TCP header's src IP address (the sender's IP addr).
 * @param src_udp_port The TCP header's src UDP port number (the sender's UDP port).
 * 
 * @returns A pointer to the TCB in the `ESTABLISHED` or `SYN_RECEIVED` state that matches the given
 * parameters. If no such TCB exists but a listening socket matches the local UTCP port, a pointer to
 * the listening socket's TCB is returned. Returns `NULL` if no matching UTCP socket is found.
 */
tcb_t* demux_tcb(
    api_t *global,
    uint16_t dest_utcp_port,
    uint32_t src_ip,
    uint16_t src_udp_port
);

#endif