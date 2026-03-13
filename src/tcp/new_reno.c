#include <tcp/congestion_control.h>
#include <tcp/tcb.h>
#include <utcp/api/tx_dgram.h>
#include <utils/logger.h>

static void newreno_init(tcb_t *tcb) {
    tcb->cwnd = MSS * IW_CALC(MSS);
    tcb->ssthresh = 0xFFFFFFFF;
    tcb->dupacks = 0;
    tcb->recover = tcb->iss;
}

static void newreno_ack_received(tcb_t *tcb, uint32_t newly_acked_bytes) {
    /* handle Slow Start, CA, and partial ACKs */
    uint32_t old_cwnd = tcb->cwnd;

    if (tcb->fast_recovery)
    {
        tcb->cwnd = tcb->ssthresh; // deflate artificially inflated window
        tcb->fast_recovery = false;
        LOG_INFO("[handle_data] RENO exited Fast Recovery. cwnd deflated to %u", tcb->cwnd);
    }
    
    
    if (tcb->cwnd < tcb->ssthresh)
    { /* Slow Start*/
        tcb->cwnd += newly_acked_bytes;
        LOG_INFO("[handle_data] SLOW START: cwnd %u -> %u", old_cwnd, tcb->cwnd);
    }
    else
    { /* Congestion Avoidance (linear growth)*/
        uint32_t increment = (newly_acked_bytes * MSS) / tcb->cwnd; // Calculate proportional growth based on how much data was ACKed
        tcb->cwnd += MAX(1, increment);
        //tcb->cwnd += (MSS * MSS) / tcb->cwnd;
        LOG_DEBUG("[handle_data] CONGESTION AVOIDANCE (linear growth): cwnd %u -> %u", old_cwnd, tcb->cwnd);
    }

    LOG_INFO("[handle_data] ACK,%u,%u", tcb->cwnd, tcb->ssthresh);
    
    const char *old_category = current_thread_cat;
    current_thread_cat = "cc_data";
    LOG_INFO("ACK,%u,%u", tcb->cwnd, tcb->ssthresh);
    current_thread_cat = old_category;

}

static void newreno_duplicate_ack(tcb_t *tcb) {
    /* Handle triple ACK Fast Retransmit / Fast Recovery */
    if (tcb->dupacks == 3)
    {
        /**
         * Only enter Fast Retransmit if snd_una is larger than
         * the previous recovery point. This ensures that we don't
         * re-enter recovery for the same window after a partial-ACK.
         */
        if (SEQ_GT(tcb->snd_una, tcb->recover))
        {
            uint32_t flight_size = tcb->snd_nxt - tcb->snd_una;
            tcb->recover = tcb->snd_max;
            tcb->ssthresh = calc_ssthresh(flight_size);

            retransmit_data(tcb, tcb->snd_una);

            tcb->cwnd = tcb->ssthresh + (3 * MSS); // inflate window by 3 MSS for 3 unACKed packets
       
        }
        else
        {
            LOG_WARN("[handle_data] NewReno: 3 duplicate ACKs but snd_una=%u <= recover=%u."
                        "Skipping fast retransmit.", tcb->snd_una, tcb->recover);
        } 
    }
    else if (tcb->dupacks > 3 && tcb->fast_recovery)
    {
        tcb->cwnd += MSS;
        LOG_DEBUG("[handle_data] RENO Fast Recovery: inflating cwnd to %u", tcb->cwnd);
        send_dgram(tcb);
    }
}

static void newreno_timeout(tcb_t *tcb) {
    /* Recalculate ssthresh, then drop cwnd to 1 MSS */
    uint32_t flight_size = tcb->snd_nxt - tcb->snd_una;
    tcb->ssthresh = calc_ssthresh(flight_size);
    tcb->cwnd = MSS;

    tcb->dupacks = 0;
    tcb->fast_recovery = false;
    tcb->recover = tcb->snd_una;
}   

// Bind the functions to the struct
const cc_ops_t cc_newreno_ops = {
    .name = "NewReno",
    .init = newreno_init,
    .ack_received = newreno_ack_received,
    .duplicate_ack = newreno_duplicate_ack,
    .timeout = newreno_timeout
};

