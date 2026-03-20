#ifndef UTCP_TIMERS_H
#define UTCP_TIMERS_H

#include <tcp/tcb.h>


#define CLOCK_GRANULARITY 1 /* 1 ms, the value of G for computing RTO (see [RFC 6298, Section 2](https://datatracker.ietf.org/doc/html/rfc6298#section-2)) */

extern const int tcp_backoff[]; /* Exponential backoff multiplier. */

/**
 * @brief Sets a timer to a given value, making certain the value is between the specified
 * min and max. This is used for the retransmission timer and persist timer.
 * 
 * Since these timers' values are calculated dynamically based on RTT they need an upper &
 * lower bounds.
 * 
 * @param tv
 * @param value
 * @param tvmin
 * @param tvmax 
 */
#define TCPT_RANGESET(tv, value, tvmin, tvmax) { \
    (tv) = (value); \
    if ((tv) < (tvmin)) \
        (tv) = (tvmin); \
    else if ((tv) > (tvmax)) \
        (tv) = (tvmax); \
}

// variables
extern const int tcp_backoff[];

/* Define functions */

/**
 * The ticker thread. Every 10ms, this thread is woken up. It loops through all of the active TCBs,
 * and within each TCB, it loops through the active timers and decrements each timer by one
 * tick. If a timer reaches 0 (expires), it is handled via a call to `utcp_timeout`.
 */
void *utcp_slowtimo_thread(void *arg);


/**
 * A wrapper function for `utcp_ticker`. Since the Timestamps Option stores 32-bit
 * values, it's easier to use a wrapper rather than call `(uint32_t)utcp_ticker()`
 * everywhere that we need it for setting timestamps.
 * 
 * This function is called in two places:
 * 
 * 1. `send_dgram()` To apply the sender's timestamp to the packet
 * 
 * 2. `calc_rto()` To get the sender's current time for calculating a packet's RTT.
 */
uint32_t tcp_now(void);

/**
 * Calculates a packet's round trip time (RTT) via Round-Trip Time Measurment (RTTM),
 * then update the corresponding TCB's retransmission timeout (RTO) value.
 * 
 * First, we calculate the RTT via a RTTM mechanism that implements the 
 * the Jacobson/Karels Algorithm (see [RFC 6298](https://datatracker.ietf.org/doc/html/rfc6298)).
 * The result is used to update the RTO.
 * 
 * @param *tcb The TCB of the current connection.
 * @param *segment_ts_ecr The TSecr value in the received segment.
 */
void calc_rto(tcb_t *tcb, uint32_t segment_ts_ecr);


/**
 * This function is called when one of our timers expires and performs necessary checks
 * and changes according to which timer expired.
 */
void utcp_timeout(tcb_t *tcb, short timer);


/**
 * Reset a timer.
 * 
 * @returns The new number of 10ms ticks until the timer times out. Function assumes that
 * it has been called  for a TCB with a lock already on it.
 * 
 * @note This currently only works for slow timers (which are decremented every 10ms).
 * In the future, when fast timers (decremented every 200ms) are implemented, the logic in
 * this function will need to be rewritten to handle both possibilities.
 * 
 * @note See [RFC 6298, Section 5.3](https://datatracker.ietf.org/doc/html/rfc6298#section-5).
 */
int reset_timer(tcb_t *tcb, uint8_t timer_idx);

/**
 * Pauses a timer to prevent it from counting down. Function assumes that it has been called 
 * for a TCB with a lock already on it.
 * 
 * @param *tcb The TCB containing the timer to pause.
 * @param timer_idx The index of the timer in `tcb->t_timer` to be paused.
 * 
 * @note See [See RFC 6298, Section 5.2](https://datatracker.ietf.org/doc/html/rfc6298#section-5).
 */
void pause_timer(tcb_t *tcb, uint8_t timer_idx);

#endif