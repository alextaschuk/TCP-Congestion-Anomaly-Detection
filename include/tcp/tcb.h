/*Transmission Control Block*/
#ifndef TCB_H
#define TCB_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#include <pthread.h>

#include <tcp/congestion_control.h>
#include <tcp/fourtuple.h>
#include <tcp/tcb_queue.h>
#include <tcp/ooo_buffer.h>
#include <utcp/api/globals.h>
#include <utcp/api/utcp_timers.h>

struct cc_ops_t;
typedef struct cc_opts_t cc_opts_t;


/* Indicates the current Congestion Avoidance state */
enum ca_state{
    OPEN = 0, /* Slow Start or CA (linear growth)*/
    RECOVERY, /* Fast Retransmit or Fast Recovery due to triple ACK */
    LOSS      /* Retransmission timer expired */
};

/* Indicates the current congestion control algorithm */
enum cc_algo{ 
TAHOE = 0,
RENO,
NEW_RENO
};


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
    uint16_t src_udp_port;  /* Actual (local) UDP port. */
    uint16_t dest_udp_port; /* Actual (remote) UDP port, included in UDP headers. */

    uint8_t fsm_state;      /* Finitie state machine's state. */
    int fd;                 /* TCB's file descriptor (a TCB is stored at `tcb_lookup[fd]`). */
    int src_udp_fd;         /* The source UDP file desriptor*/

    /* Connection and data variables */
    uint32_t iss;           /* Inital send sequence number */
    uint32_t snd_una;       /* Oldest unacked sequence number. */
    uint32_t snd_nxt;       /* Next sequence number to send. */
    uint32_t snd_max;       /* Highest sequence number sent. */
    uint32_t snd_wnd;       /* Total amount of free space in the `tx_buffer`, in bytes. (peer's rwnd) */
    uint32_t irs;           /* Initial receive sequence number. */
    uint32_t rcv_nxt;       /* Next expected sequence number. */
    uint32_t rwnd;       /* Amount of free space in the `rx_buffer`, in bytes. (i.e., amt of data receiver will accept). */

    /* Window scaling */
    bool ws_enabled;        /* `True` if window scaling is enabled (the peer included the window scale option in their SYN). */
    uint8_t snd_ws_scale;   /* The peer's window scale. (value from incoming SYN) */
    uint8_t rcv_ws_scale;   /* The host's window scale. (value in outgoing SYN) */
    
    uint16_t t_flags;         /* Internal TCB control flags necessary for forcing certain things. */
    #define F_ACKNOW 0x0001   /* Send an immediate ACK segment with an empty payload. */
    #define F_DELACK 0x0002   /* Delayed ACK pending; TCPT_DELACK timer is running. */

    /* Congestion Avoidance*/
    
    /**
     * Round Trip Time (RTT) and Retransmission Timeout (RTO) variables.
     * - We use TSval and TSecr, as defined in https://www.rfc-editor.org/rfc/rfc1323#section-3 for calculating a packet's RTT.
     */
    uint32_t ts_rcv_val;    /* Stores the peer's TSval timestamp that is included in a received packet (when you send a packet, this value is used for `TSecr`)*/

    /* RTT Calculation (see RFC 6298, Section 2) */
    uint32_t srtt;          /* Smoothed RTT (avg RTT) -- scaled by 8 */
    uint32_t rttvar;        /* round trip time variation -- scaled by 4 */
    uint32_t rxtcur;        /* Current retransmit timeout, RTO (ticks) */
    uint8_t rxtshift;       /* Number of retransmission timers that have expired. Used to calculate the backoff multiplier when a packet is retransmitted. */


    /* Congestion Control */
    uint32_t cwnd;                  /* Congestion window */
    uint32_t ssthresh;              /* Slow start threshold */
    uint8_t dupacks;                /* Counter for the number of consecutive duplicate ACKs received */
    const struct cc_ops_t *cc;      /* The CC handler. */
    enum ca_state ca_state;         /* The current CA state. */
    uint32_t recover;               /* Sequence number to reach before exiting Fast Recovery. This is the highest sequence number that was sent at the moment a packet is lost.*/

    //short tcpt_rexmt;       /* Retransmission timer (Retransmission eXaimt) */

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
    pthread_mutex_t lock;       /* Lock to prevent race conditions when reading and writing to `rx` & `tx` buffers */ 
    pthread_cond_t conn_cond;   /* Client-only condition variable used to block after a 3WHS has been initiated and is waiting for a SYN-ACK */

    /* Transmit (Send) Buffer*/
    uint8_t tx_buf[BUF_SIZE];   /* Transmit (or "send") buffer, which stores unacked bytes that have been sent out. */
    uint32_t tx_head;           /* Index to read from. */
    uint32_t tx_tail;           /* Index to write to.  */

    /* Receive Buffer*/
    uint8_t rx_buf[BUF_SIZE];   /* Receive buffer, which stores acked bytes that you have received and sent ACK for. */
    uint32_t rx_head;           /* Index to read from. */
    uint32_t rx_tail;           /* Index to write to.  */
    
    /* Out-of-Order Buffer*/
    ooo_segment_t *ooo_head;    /* Head of the out-of-order buffer. */
    uint32_t ooo_bytes;         /* Total number of bytes in the OOO buffer. */

    /* Server-only buffers for 3WHS management*/
    tcb_queue_t syn_q;          /* Server-only queue for tracking half-open connection requests. */
    tcb_queue_t accept_q;       /* Server-only queue for tracking connection requests that are complete, but have not yet been `accept()`ed by the app. */
} tcb_t;



#endif
