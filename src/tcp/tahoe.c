#include <tcp/congestion_control.h>
#include <tcp/tcb.h>
#include <utcp/api/tx_dgram.h>
#include <utils/logger.h>
#include <utils/err.h>


static void tahoe_ack_received(tcb_t *tcb, uint32_t newly_acked_bytes)
{
    tcb->ca_state = OPEN;
    cc_aimd(tcb, newly_acked_bytes); // no fast recovery state, only AIMD for valid ACKs.
}


static void tahoe_duplicate_ack(tcb_t *tcb)
{
    if (tcb->dupacks == 3)
    {
        /* Tahoe treats a triple ACK the same as it does a timeout */
        uint32_t flight_size = tcb->snd_nxt - tcb->snd_una;
        tcb->ssthresh = halve_ssthresh(flight_size);
        tcb->cwnd = MSS;

        retransmit_data(tcb, tcb->snd_una);

        LOG_WARN("[tahoe_duplicate_ack] Fast Retransmit: flight=%u, ssthresh=%u, cwnd=%u"
            " recover=%u", flight_size, tcb->ssthresh, tcb->cwnd, tcb->recover);
            
        const char *old_category = current_thread_cat;
        current_thread_cat = "cc_data";
        LOG_INFO("TRIPLE_ACK,%u,%u", tcb->cwnd, tcb->ssthresh);
        current_thread_cat = old_category;
    }
}


static void tahoe_timeout(tcb_t *tcb, uint32_t flight_size)
{
    cc_rexmt_timeout(tcb, flight_size);

    const char *old_category = current_thread_cat;
    current_thread_cat = "cc_data";
    LOG_INFO("TIMEOUT,%u,%u", tcb->cwnd, tcb->ssthresh);
    current_thread_cat = old_category;
}   


const cc_ops_t cc_tahoe_ops =
{
    .name = "tahoe",
    .init = cc_init,
    .ack_received = tahoe_ack_received,
    .duplicate_ack = tahoe_duplicate_ack,
    .timeout = tahoe_timeout
};

