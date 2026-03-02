#ifndef API_H
#define API_H
/**
 * This API contains functions, variables, etc. that are used 
 * by the server and the client.
 */

#include <stdint.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <tcp/hndshk_fsm.h>
#include <tcp/tcb.h>

/*Begin function declarations*/

/**
 * @brief Deserialize a UDP datagram's payload
 * into a TCP header and data into host byte order
 * 
 * @param *buf the buffer of received data (TCP header + payload)
 * @param buflen how many bytes make up buf (rcvsize)
 * @param **out_hdr deserialized TCP header (caller-allocated)
 * @param **out_data deserialized data payload (caller-allocated)
 * @param *out_data_len how many bytes make up the data payload
 * 
 * @note This is a destructive function.
 */
void deserialize_utcp_packet
(
    uint8_t *buf,
    size_t buflen,
    struct tcphdr **out_hdr,
    uint8_t **out_data,
    ssize_t *out_data_len
);

/**
 * Update a TCB's FSM state.
 * 
 * @param utcp_fd The UTCP file descriptor of the TCB whose state is being updated.
 * @param state The new state of the TCB.
 */
void update_fsm(int utcp_fd, enum conn_state state);

/**
 * @brief Retrieve the TCB for a UTCP socket at `tcb_lookup[pos]`
 * 
 * @param utcp_fd A UTCP file descriptor.
 * 
 * @returns A pointer to the TCB struct, or `NULL` if `utcp_fd` is invalid
 * or a TCB is not found.
 */
struct tcb_t *get_tcb(int utcp_fd);

/*End function declarations*/

#endif
