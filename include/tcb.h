/**
 * Transmission Control Block
 */
#include <stdint.h>
#include <fourtuple.h>

#ifndef TCB_H
#define TCB_H

typedef struct tcb
{
    //standard 4-tuple, included in TCP header
    fourtuple fourtuple;
    
    // actual UDP port, included in UDP header
    uint16_t dest_udp_port;
    
    uint8_t fsm_state; // finitie state machine's state

    uint32_t iss;     /* the inital send sequence */
    uint32_t snd_una; /* oldest unack sequence number */
    uint32_t snd_nxt; /* the next sequence number to send */
    uint32_t rcv_nxt; /* the next expected sequence */
    uint32_t irs;     /* initial recv seq */
}tcb;


#endif