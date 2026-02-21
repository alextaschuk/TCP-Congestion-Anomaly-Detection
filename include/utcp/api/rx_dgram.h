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
 * @brief receive a datagram and update TCB values as needed.
 * 
 * @param udp_fd The UDP listen socket's FD.
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
 * @param *listen_tcb The listen socket's TCB
 * @param udp_sock The sender's UDP socket FD
 * @param *hdr The TCP header of the received packet
 * @param data_len The length of the TCP packet's payload (throws `err_data` if this is > 0)
 * @param from Contains info about who sent the SYN
 */
static void rcv_syn(
    tcb_t *listen_tcb,
    int udp_sock,
    struct tcphdr *hdr,
    ssize_t data_len,
    struct sockaddr_in from
);

/**
 * Handles a SYN-ACK packet and sends an
 * ACK to the sender in response.
 */
static void rcv_syn_ack(
    tcb_t *tcb,
    int udp_sock,
    struct tcphdr *hdr,
    ssize_t data_len
);

/** 
 * @brief Handles the ACK packet that completes the 3WHS.
 */
static void rcv_ack(
    tcb_t *listen_tcb,
    struct tcphdr *hdr,
    ssize_t data_len,
    struct sockaddr_in from
);
/**
 * Handles a packet that contains a data payload to a destination with an established connection.
 */
static void handle_data(
    tcb_t *tcb,
    int udp_sock,
    struct tcphdr *hdr, 
    uint8_t *data,
    ssize_t data_len,
    ssize_t buflen
);

/**
 * Searches for a TCB that is either active (FSM state is `ESTABLISHED`), or a half-open connection
 * (FSM state is `SYN_RECEIVED`) using the remote sender's info and the local UTCP port.
 * 
 * @param *global A pointer to the global `api_t` instance.
 * @param local_utcp_port The dest UTCP port number in the TCP header (server's UTCP port).
 * @param remote_ip The src IP address in the TCP header (client's IP addr).
 * @param remote_udp_port The src UDP port number in the TCP header (client's UDP port).
 * 
 * @returns A pointer to the TCB in the `ESTABLISHED` or `SYN_RECEIVED` state
 * that matches the given parameters. If no such TCB exists but a listening
 * socket matches the local UTCP port, a pointer to the listening socket's
 * TCB is returned. Returns `NULL` if no matching UTCP socket is found.
 */
tcb_t* demux_tcb(api_t *global, uint16_t local_utcp_port, uint32_t remote_ip, uint16_t remote_udp_port);

#endif