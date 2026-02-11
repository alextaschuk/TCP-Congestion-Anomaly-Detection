/**
 * Transmission Control Block
 */
#ifndef TCB_H
#define TCB_H

#include <stdint.h>

#include <tcp/fourtuple.h>
#include <utcp/api/data.h>
#include <utcp/api/ring_buffer.h>

typedef struct tcb_t
{
    //standard 4-tuple, included in TCP header
    fourtuple fourtuple;
    
    // actual UDP port, included in UDP header
    uint16_t dest_udp_port;
    
    uint8_t fsm_state; // finitie state machine's state

    uint32_t iss;     /* the inital send sequence number */
    uint32_t snd_una; /* oldest unacked sequence number */
    uint32_t snd_nxt; /* the next sequence number to send */
    uint32_t rcv_nxt; /* the next expected sequence */
    uint32_t irs;     /* initial recv seq */

    uint16_t rcv_wnd; // amt of data receiver will accept

    ring_buf_t tx_buf; // (send (transmit) buffer) store unacked bytes that have been sent out
    ring_buf_t rx_buf; // (receive buffer) store acked bytes that you have received and sent ack for
}tcb_t;

#endif
