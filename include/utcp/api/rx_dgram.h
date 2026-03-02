/**
 * Logic to receive and handle a datagram according
 * to the receiver's current (FSM) connection state
 */
#ifndef RX_DGRAM_H
#define RX_DGRAM_H

#include <arpa/inet.h>
#include <stdint.h>
#include <sys/types.h>

#include <tcp/tcb.h>
#include <utcp/api/data.h>
#include <utils/err.h>
#include <utils/printable.h>
#include <utcp/api/globals.h>


 /**
 * @brief Receive a datagram and update TCB values as needed.
 * 
 * @param udp_fd The receiving UDP socket's FD.
 * @param buflen The number of bytes of the datagram payload
 * 
 * @note For a UDP socket to receive a datagram, it needs to call this function. 
 */
ssize_t rcv_dgram(int udp_fd, ssize_t buflen);

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
static void rcv_syn(
    tcb_t *listen_tcb,
    int udp_fd,
    struct tcphdr *hdr,
    ssize_t data_len,
    struct sockaddr_in from
);

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
static void rcv_syn_ack(
    tcb_t *tcb,
    int udp_fd,
    struct tcphdr *hdr,
    ssize_t data_len
);

/** 
 * @brief Handles the ACK packet that completes the 3WHS.
 * 
 * This function should not be confused with `handle_ack()`, which should only be
 * called when an ACK is received on a connection that is in the `ESTABLISHED` state.
 * 
 * @param *listen_tcb The server's listen TCB.
 * @param *hdr The received TCP header.
 * @param from Contains info about who sent the SYN.
 */
static void rcv_3whs_ack(
    tcb_t *listen_tcb,
    struct tcphdr *hdr,
    struct sockaddr_in from
);
/**
 * Handles a TCP segment that contains a payload for a UTCP socket that is in the `ESTABLISHED` state.
 * 
 * @param *tcb A server TCB of the UTCP socket that is receiving the segment.
 * @param udp_fd The server's UDP socket to send data (e.g. ACK) out of.
 * @param *hdr The received TCP segment's header.
 * @param *data The received TCP segment's payload.
 * @param data_len The size of `*data`, in bytes.
 */
static void handle_data(
    tcb_t *tcb,
    int udp_fd,
    struct tcphdr *hdr, 
    uint8_t *data,
    ssize_t data_len
);

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
static void handle_ack(tcb_t *tcb, struct tcphdr *hdr);

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

/**
 * Search for timestamps in the Options section of a TCP header. Since this section can have a variable
 * length of up to 40 bytes, we need to see how many bytes of options exist so that we don't read into
 * the payload.
 * 
 * @param *hdr The received TCP header that we are searching through.
 * @param *ts_val If/when TSval is found, it is stored here.
 * @param *ts_ecr If/when TSecr is found, it is stored here.
 * 
 * @returns `true` if valid TSval and TSecrl values are found, `false` otherwise.
 */
static bool find_timestamps(struct tcphdr *hdr, uint32_t *ts_val, uint32_t *ts_ecr);

#endif