#include <utcp/rx/handle_ack.h>

#include <stdio.h>

#include <utils/logger.h>
#include <utcp/api/globals.h>
#include <utcp/api/tx_dgram.h>


void handle_ack(tcb_t *tcb, struct tcphdr *hdr)
{   
    LOG_INFO("entered handle_ack");
    uint32_t ack = hdr->th_ack;

    if (ack < tcb->snd_una)
    {
        LOG_WARN("[handle_ack] Ignoring stale ACK");
        return;
    }

    else if (ack > tcb->snd_nxt)
    {
        LOG_WARN("[handle_ack] Ignoring invalid future ACK (too large)");
        return;
    }

    /* Potential duplicate ACK */
    else if (ack == tcb->snd_una)
    {
        if (tcb->snd_nxt > tcb->snd_una) // no new data has been ACKed
        {
            tcb->dupacks++;
            LOG_INFO("[handle_ack] Received duplicate ACK. Total count: %u", tcb->dupacks);
        }
        
        /* Three duplicate ACKs detected; handle according to the current CA algorithm. */
        if (tcb->dupacks == 3)
        {
            /* both Tahoe and RENO use fast retransmit, then set ssthresh to 50% of cwnd */
            LOG_INFO("[handle_ack] 3 Duplicate ACKs. Retransmitting sequence: %u\n and applying TCP %s for congestion avoidance", tcb->snd_una, CA_ALGO);

            retransmit_data(tcb);
            uint32_t flight_size = tcb->snd_nxt - tcb->snd_una;
            tcb->snd_ssthresh = MAX(flight_size / 2, 2 * MSS);

            switch (CA_ALGO)
            {
                case (TAHOE): // drop cwnd to 1 MSS and re-enter slow start.
                    tcb->snd_cwnd = MSS;
                    break;
                case(RENO): // 
                    tcb->fast_recovery = true;
                    tcb->snd_cwnd = tcb->snd_ssthresh + (3 * MSS);
                    LOG_INFO("[handle_ack] RENO: Entered Fast Recovery. cwnd set to %u", tcb->snd_cwnd);
                    break;
            }
        }

        /**
        * RENO Fast Recovery: For every additional duplicate ACK, we know another packet has left the network,
        * so we need ot inflate the window by 1 MSS since we know that it won't be delivered.
        */
        else if (tcb->dupacks > 3 && CA_ALGO == RENO && tcb->fast_recovery)
        {
                tcb->snd_cwnd += MSS;
                LOG_INFO("[handle_ack] Reno: Inflating cwnd to %u", tcb->snd_cwnd);
        }
    }

    /* ACK is valid */
    else
    {
        LOG_INFO("[handle_ack] Received valid ACK packet\n");
        size_t acked = (size_t)(ack - tcb->snd_una);

        tcb->dupacks = 0; // reset counter

        if (tcb->fast_recovery)
        { // (RENO only) We can now exit Fast Recovery
            tcb->snd_cwnd = tcb->snd_ssthresh;
            tcb->fast_recovery = false;
            LOG_INFO("[handle_ack] Reno: Exited Fast Recovery. cwnd deflated to %u", tcb->snd_cwnd);
        }
        else
        { // Standard cwnd growth logic (slow start or linear)
            if (tcb->snd_cwnd < tcb->snd_ssthresh)
            { // connection is in slow start
                uint32_t old_cwnd = tcb->snd_cwnd;
                tcb->snd_cwnd += MSS;
                LOG_INFO("[handle_ack] Slow Start cwnd grew from %u to %u", old_cwnd, tcb->snd_cwnd);
            }
            else // congestion avoidance: linear growth
                tcb->snd_cwnd += (MSS * MSS) / tcb->snd_cwnd;
        }

        //LOG_INFO("METRIC,%u,%u,%u\n", tcp_now(), tcb->snd_cwnd, tcb->snd_ssthresh);

        uint32_t old_snd_una = tcb->snd_una;
        uint32_t old_snd_wnd = tcb->snd_wnd;
        tcb->snd_una = ack;
        tcb->snd_wnd = hdr->th_win;
        LOG_INFO("[handle_ack] snd_una updated from %u to %u", old_snd_una, tcb->snd_una);
        LOG_INFO("[handle_ack] snd_wnd updated from %u to %u", old_snd_wnd, tcb->snd_wnd);

        /**
         * Deliver bytes from the TX buffer to the app, then tell the sleeping app's
         * thread that there is now room in the TX, so it can wake up (in utcp_send())
         * and send more data.
         */
        ring_buf_read(&tcb->tx_buf, NULL, acked);
        pthread_cond_broadcast(&tcb->conn_cond);

        /* Pause or reset the timer */
        if (tcb->snd_una == tcb->snd_nxt)
        {
            LOG_INFO("[handle_ack] All data has been ACKed. Pausing REXMT timer.");
            pause_timer(tcb, TCPT_REXMT); 
        }
        else
        {
            int ticks = reset_timer(tcb, TCPT_REXMT);
            LOG_INFO("[handle_ack] UnACKed data in flight. REXMT timer reset to %d ticks.", ticks);
        }
    }
}

