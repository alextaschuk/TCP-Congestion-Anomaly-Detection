/*Transmission Control Block*/
#ifndef TCB_H
#define TCB_H

#include <stdint.h>
#include <stdbool.h>

#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <pthread.h>

#include <tcp/fourtuple.h>
#include <tcp/tcb_queue.h>
#include <utcp/api/ring_buffer.h>
#include <utcp/api/utcp_timers.h>


/**
 * @brief A TCP Control Block, or Transmission Control Block (TCB).
 * 
 * A TCB contains all necessary info to maintain a connection. Each connection requires two TCBs: one that is
 * managed by the sender, and one that is managed by the receiver.
 * 
 * @note ALL TCB DATA SHOULD BE STORED IN HOST ORDER.
 */
typedef struct tcb_t
{
    /* Connection Info */
    fourtuple_t fourtuple;  /* A standard 4-tuple, included in the TCP header. */
    uint16_t dest_udp_port; /* Actual (remote) UDP port, included in UDP headers. */
    uint16_t src_udp_port;  /* Actual (local) UDP port. */

    uint8_t fsm_state;      /* Finitie state machine's state. */
    int fd;                 /* TCB's file descriptor (a TCB is stored at `tcb_lookup[fd]`). */

    /* Connection and data variables */
    uint32_t iss;           /* inital send sequence number */
    uint32_t snd_una;       /* oldest unacked sequence number. */
    uint32_t snd_nxt;       /* next sequence number to send. */
    uint32_t snd_wnd;       /* Total amount of free space in the `tx_buffer`, in bytes. (peer's rcv_wnd) */
    uint32_t irs;           /* initial receive sequence number. */
    uint32_t rcv_nxt;       /* next expected sequence number. */
    uint32_t rcv_wnd;       /* amount of free space in the `rx_buffer`, in bytes. (i.e., amt of data receiver will accept) */
    
    /* Congestion Avoidance*/
     /* Round Trip Time (RTT) and Retransmission Timeout (RTO) variables */
     // we use TSval and TSecr, as define in https://www.rfc-editor.org/rfc/rfc1323#section-3 for calculating a packet's RTT.
    uint32_t ts_rcv_val;    /* Stores the peer's TSval timestamp that is included in a received packet (when you send a packet, this value is used for `TSecr`)*/
    uint32_t rto;           /* Current Retransmission Timeout value */

    /* RTT Calculation (see RFC 6298, Section 2) */
    uint32_t srtt;          /* Smoothed RTT (avg RTT) -- scaled by 8 */
    uint32_t rttvar;        /* round trip time variation -- scaled by 4 */


    /* Congestion Control */
    uint32_t snd_cwnd;      /* congestion window */
    uint32_t snd_ssthresh;  /* slow start threshold */
    uint8_t dupacks;        /* counter for the number of consecutive duplicate ACKs received */
    bool fast_recovery;     /* `true` if `CA_ALGO == TAHOE` AND 3 duplicate ACKs have been detected, `false` otherwise */

    int rxtcur;             /* Current retransmit timeout, RTO (ticks) */
    u_int rttmin;           /* minimum value for retransmissino timeout allowed */
    short tcpt_rexmt;       /* retransmission timer (counter) */

    /**
     * Each entry in this array contains the number of 500ms or 200ms clock ticks until the timer expires, with `0` meaning that the timer is not set.
     * These four counters (macros in `globals.h`) are used to implement six timers:
     * - `TCPT_REXMT`: retransmission timer
     * - `TCPT_PERSIST`: persist timer
     * - `TCPT_KEEP`: keepalive timer *or* connection-establishment timer
     * - `TCPT_2MSL`: 2MSL timer *or* FIN_WAIT_2 timer
     */
    short t_timer[TCPT_NTIMERS];

    /* Threading synchronization*/
    pthread_mutex_t lock;       /* lock to prevent race conditions when reading and writing to `rx` & `tx` buffers */ 
    pthread_cond_t conn_cond;   /* client-only condition variable used to block after a 3WHS has been initiated and is waiting for a SYN-ACK */

    /* Send and recieve buffers */
    ring_buf_t tx_buf;          /* transmit (or "send") buffer, which stores unacked bytes that have been sent out */
    ring_buf_t rx_buf;          /* receive buffer, which stores acked bytes that you have received and sent ACK for */

    /* Server-only buffers for 3WHS management*/
    tcb_queue_t syn_q;          /* server-only queue for tracking half-open connection requests. */
    tcb_queue_t accept_q;       /* server-only queue for tracking connection requests that are complete, but have not yet been `accept()`ed by the app. */
} tcb_t;



#endif
