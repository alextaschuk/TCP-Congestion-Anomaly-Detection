#ifndef CONGESTION_CONTROL_H
#define CONGESTION_CONTROL_H

#include <stdint.h>

#include <tcp/tcb.h>

struct tcb_t;
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
    void (*timeout)(tcb_t *tcb, uint32_t flight_size);
    
} cc_ops_t;

// Expose the available algorithms
extern const cc_ops_t cc_reno_ops;
extern const cc_ops_t cc_newreno_ops;

/*Define functions*/

/**
 * Initializes the `cwnd` and `ssthresh` for our
 * congestion control algorithm.
 */
static void cc_init(tcb_t *tcb);

/**
 * When a triple ACK or timeout occurs, the ssthresh needs to
 * be recalculated. This sets ssthresh `MAX(50% of bytes in flight, 2 MSS)`
 */
uint32_t halve_ssthresh(uint32_t flight_size);

/**
 * Additive Increase/Multiplicative Decrease. Handles Slow Start and Congestion Avoidance.
 * - Additive Increase: Increase cwnd by 1 MSS every RTT until loss detected
 * - Multiplicative Decrease: Cut cwnd in half after loss 
 */
void cc_aimd(tcb_t *tcb, uint32_t acked);

/**
 * Handles a packet timeout when the retransmission timer expires.
 * - Recalculate ssthresh, then drop cwnd to 1 MSS
 */
void cc_rexmt_timeout(tcb_t *tcb, uint32_t flight_size);


/*End define functions*/

#endif