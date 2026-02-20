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


 /**
 * @brief receive a datagram and update TCB values as needed.
 * 
 * @note For a UDP socket to receive a datagram, it needs to call this function. 
 */
ssize_t rcv_dgram(int sock, tcb_t *tcb, ssize_t buflen);

/**
 * Handles a SYN packet and sends a SYN-ACK
 * to the sender in response.
 */
static void rcv_syn(
    tcb_t *tcb,
    int udp_sock,
    uint8_t *buf,
    ssize_t rcvsize,
    struct tcphdr *hdr,
    uint8_t *data,
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
 * Handles the ACK packet that completes
 * the 3WHS.
 */
static void rcv_ack(
    tcb_t *tcb,
    struct tcphdr *hdr,
    ssize_t data_len
);
/**
 * Handles a packet that contains a data
 * payload to a destination with an 
 * established connection.
 */
static void handle_data(
    tcb_t *tcb,
    int udp_sock,
    struct tcphdr *hdr, 
    uint8_t *data,
    ssize_t data_len,
    ssize_t buflen
);

#endif