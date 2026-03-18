#include <tcp/congestion_control.h>
#include <tcp/tcb.h>
#include <utcp/api/tx_dgram.h>
#include <utils/logger.h>
#include <utils/err.h>


static void reno_ack_received(tcb_t *tcb, uint32_t newly_acked_bytes)
{
    /* handle Slow Start, CA, and partial ACKs */
    if (tcb->ca_state == RECOVERY)
    {
        tcb->cwnd = tcb->ssthresh;
        tcb->ca_state = OPEN;
        
        const char *old_category = current_thread_cat;
        current_thread_cat = "cc_data";
        LOG_INFO("ACK,%u,%u", tcb->cwnd, tcb->ssthresh);
        current_thread_cat = old_category;
    }
    else
    {
        cc_aimd(tcb, newly_acked_bytes);
    }
}


static void reno_duplicate_ack(tcb_t *tcb)
{
    if (tcb->dupacks == 3)
    {
        /* Update ssthresh, perform Fast Retransmit, then enter Fast Recovery. */
        uint32_t flight_size = tcb->snd_nxt - tcb->snd_una;
        tcb->ssthresh = halve_ssthresh(flight_size);
        
        retransmit_data(tcb, tcb->snd_una); // fast retransmit

        // enter fast recovery
        tcb->ca_state = RECOVERY;
        tcb->cwnd = tcb->ssthresh + 3 * MSS; // inflate window by 3 MSS for 3 unACKed packets

        LOG_WARN("[reno_duplicate_ack] Fast Retransmit: flight=%u, ssthresh=%u, cwnd=%u"
            " recover=%u", flight_size, tcb->ssthresh, tcb->cwnd, tcb->recover);
            
        const char *old_category = current_thread_cat;
        current_thread_cat = "cc_data";
        LOG_INFO("TRIPLE_ACK,%u,%u", tcb->cwnd, tcb->ssthresh);
        current_thread_cat = old_category;
    }
    else if (tcb->dupacks > 3 && tcb->ca_state == RECOVERY)
    {
        tcb->cwnd += MSS;
        LOG_DEBUG("[reno_duplicate_ack] reno: Fast Recovery inflated cwnd to %u", tcb->cwnd);
        send_dgram(tcb);
    }
}


static void reno_timeout(tcb_t *tcb, uint32_t flight_size)
{
    cc_rexmt_timeout(tcb, flight_size);

    const char *old_category = current_thread_cat;
    current_thread_cat = "cc_data";
    LOG_INFO("TIMEOUT,%u,%u", tcb->cwnd, tcb->ssthresh);
    current_thread_cat = old_category;
}   


const cc_ops_t cc_reno_ops =
{
    .name = "reno",
    .init = cc_init,
    .ack_received = reno_ack_received,
    .duplicate_ack = reno_duplicate_ack,
    .timeout = reno_timeout
};

