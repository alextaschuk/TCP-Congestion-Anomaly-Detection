#include <tcp/congestion_control.h>

#include <stdint.h>

#include <utcp/api/globals.h>
#include <utils/logger.h>

void cc_init(tcb_t *tcb)
{
    tcb->cwnd = MSS * IW_CALC(MSS);
    tcb->ssthresh = 0xFFFFFFFF; // should be arbitrarily high, see RFC 5681, Section 3.1
    tcb->ca_state = OPEN;
    tcb->recover = tcb->iss;

    if (tcb->fd != 0)
    { /* We don't want to print this when the the listen socket's TCB is made. */
    const char *old_category = current_thread_cat;
    current_thread_cat = "cc_data";
    LOG_INFO("INIT,%u,%u", tcb->cwnd, tcb->ssthresh);
    current_thread_cat = old_category;
    }
}

uint32_t halve_ssthresh(uint32_t flight_size)
{
    uint32_t half_flight = flight_size / 2;
    return MAX(half_flight, 2 * MSS);
}

void cc_aimd(tcb_t *tcb, uint32_t acked)
{
    if (tcb->ca_state == LOSS)
    {
        tcb->ca_state = OPEN;
    }

    uint32_t old_cwnd = tcb->cwnd;
    
    if (tcb->cwnd < tcb->ssthresh)
    { /* Slow Start */
        tcb->cwnd += MIN(acked, MSS);
        LOG_INFO("[handle_data] SLOW START: cwnd %u -> %u", old_cwnd, tcb->cwnd);
    }
    else 
    { /* Congestion Avoidance (linear growth) */
        tcb->cwnd += MAX(((uint64_t)acked * MSS) / tcb->cwnd, 1);
        LOG_DEBUG("[handle_data] CONGESTION AVOIDANCE (linear growth): cwnd %u -> %u", old_cwnd, tcb->cwnd);
    }

    const char *old_category = current_thread_cat;
    current_thread_cat = "cc_data";
    LOG_INFO("ACK,%u,%u", tcb->cwnd, tcb->ssthresh);
    current_thread_cat = old_category;
}


void cc_rexmt_timeout(tcb_t *tcb, uint32_t flight_size)
{
    tcb->ssthresh = halve_ssthresh(flight_size);
    tcb->cwnd = MSS;
    tcb->ca_state = LOSS;
}