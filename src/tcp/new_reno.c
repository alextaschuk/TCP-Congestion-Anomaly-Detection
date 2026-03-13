#include <tcp/congestion_control.h>
#include <tcp/tcb.h>
#include <utcp/api/tx_dgram.h>
#include <utils/logger.h>

static void newreno_init(tcb_t *tcb) {
    tcb->cwnd = MSS * IW_CALC(MSS);
    tcb->ssthresh = 0xFFFFFFFF;
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

static void newreno_ack_received(tcb_t *tcb, uint32_t newly_acked_bytes) {
    /* handle Slow Start, CA, and partial ACKs */
    uint32_t old_cwnd = tcb->cwnd;

    if (tcb->ca_state == RECOVERY)
    {
        if (SEQ_GEQ(tcb->snd_una, tcb->recover))
        { /* Full ACK. We can exit fast recovery */
            tcb->cwnd = tcb->ssthresh; // deflate artificially inflated window
            tcb->ca_state = OPEN;
            LOG_INFO("[handle_data] NewReno: Full ACK; exited Fast Recovery. cwnd deflated to %u",
                            tcb->cwnd);

            const char *old_category = current_thread_cat;
            current_thread_cat = "cc_data";
            LOG_INFO("ACK,%u,%u", tcb->cwnd, tcb->ssthresh);
            current_thread_cat = old_category;
        }
        else
        { /* Partial ACK */
            retransmit_data(tcb, tcb->snd_una);

            tcb->cwnd = (newly_acked_bytes >= tcb->cwnd) ? MSS :tcb->cwnd - newly_acked_bytes;
            tcb->cwnd += MSS;
            tcb->dupacks = 0;

            LOG_INFO("[handle_data] NewReno: Partial ACK retransmitting %u. cwnd=%u, recover=%u",
                          tcb->snd_una, tcb->cwnd, tcb->recover);

            const char *old_category = current_thread_cat;
            current_thread_cat = "cc_data";
            LOG_INFO("PARTIAL_ACK,%u,%u", tcb->cwnd, tcb->ssthresh);
            current_thread_cat = old_category;
        }
    }
    else
    {
        cc_aimd(tcb, newly_acked_bytes);
    }
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
            tcb->ssthresh = halve_ssthresh(flight_size);

            retransmit_data(tcb, tcb->snd_una);

            tcb->ca_state = RECOVERY;
            tcb->cwnd = tcb->ssthresh + (3 * MSS); // inflate window by 3 MSS for 3 unACKed packets

            LOG_WARN("[newreno_duplicate_ack] Fast Retransmit: flight=%u, ssthresh=%u, cwnd=%u"
                        "recover=%u", flight_size, tcb->ssthresh, tcb->cwnd, tcb->recover);
            
            const char *old_category = current_thread_cat;
            current_thread_cat = "cc_data";
            LOG_INFO("TRIPLE_ACK,%u,%u", tcb->cwnd, tcb->ssthresh);
            current_thread_cat = old_category;
        }
        else
        {
            LOG_WARN("[handle_data] NewReno: 3 duplicate ACKs but snd_una=%u <= recover=%u."
                        "Skipping fast retransmit.", tcb->snd_una, tcb->recover);
        } 
    }
    else if (tcb->dupacks > 3 && tcb->ca_state == RECOVERY)
    {
        tcb->cwnd += MSS;
        LOG_DEBUG("[handle_data] NewReno: Fast Recovery inflating cwnd to %u", tcb->cwnd);
        send_dgram(tcb);
    }
}

static void newreno_timeout(tcb_t *tcb, uint32_t flight_size) {
    
    tcb->recover = tcb->snd_max; // allows next triple ACK to trigger fast retransmit quickly.
    cc_rexmt_timeout(tcb, flight_size);

    tcb->ssthresh = halve_ssthresh(flight_size);

    const char *old_category = current_thread_cat;
    current_thread_cat = "cc_data";
    LOG_INFO("TIMEOUT,%u,%u", tcb->cwnd, tcb->ssthresh);
    current_thread_cat = old_category;
}   

// Bind the functions to the struct
const cc_ops_t cc_newreno_ops = {
    .name = "NewReno",
    .init = newreno_init,
    .ack_received = newreno_ack_received,
    .duplicate_ack = newreno_duplicate_ack,
    .timeout = newreno_timeout
};

