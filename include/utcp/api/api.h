#ifndef API_H
#define API_H

#include <stdint.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <tcp/hndshk_fsm.h>
#include <tcp/tcb.h>

/*Begin function declarations*/

/**
 * @brief deserialize a TCP header into host byte order
 * 
 * @param *buf the buffer of received data (TCP header + payload)
 * @param buflen how many bytes make up buf (rcvsize)
 * @param **out_hdr deserialized TCP header
 * @param **out_data deserialized data payload (stream of bytes)
 * @param *out_data_len how many bytes make up the data payload
 */
void deserialize_tcp_hdr(uint8_t *buf, size_t buflen, struct tcphdr **out_hdr, uint8_t **out_data, ssize_t *out_data_len);

/**
 * @brief updates the FSM state of 
 * a TCB using the passed in UTCP fd
 */
void update_fsm(int utcp_fd, enum conn_state state);

/**
 * @brief Retrieves the TCB for a UTCP socket at tcb_lookup[pos]
 * 
 * @details Returns a pointer to the TCB struct
 * using the passed in UTCP fd
 */
struct tcb_t *get_tcb(int utcp_fd);


/*End function declarations*/

#endif
