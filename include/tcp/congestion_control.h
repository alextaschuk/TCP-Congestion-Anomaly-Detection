#ifndef CONGESTION_CONTROL_H
#define CONGESTION_CONTROL_H

#include <stdint.h>

// Forward declaration so the ops can take the TCB as a parameter
typedef struct tcb_t tcb_t; 

/**
 * The Congestion Control Interface. Every algorithm (Reno, NewReno,etc.) will implement these functions.
 */
typedef struct cc_ops_t {
    const char *name; // e.g., "NewReno"
    
    // Called when a new TCB is created
    void (*init)(tcb_t *tcb);
    
    // Called when a valid, in-order ACK is received (Congestion Avoidance / Slow Start)
    void (*ack_received)(tcb_t *tcb, uint32_t newly_acked_bytes);
    
    // Called when a duplicate ACK is received
    void (*duplicate_ack)(tcb_t *tcb);
    
    // Called when the Retransmission Timer (RTO) expires
    void (*timeout)(tcb_t *tcb);
    
} cc_ops_t;

// Expose the available algorithms
extern const cc_ops_t cc_reno_ops;
extern const cc_ops_t cc_newreno_ops;

#endif