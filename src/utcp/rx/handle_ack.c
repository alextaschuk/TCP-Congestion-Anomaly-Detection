#include <utcp/rx/handle_ack.h>

#include <stdio.h>

#include <utils/logger.h>
#include <utcp/api/globals.h>
#include <utcp/api/tx_dgram.h>


void handle_ack(tcb_t *tcb, struct tcphdr *hdr, ssize_t data_len)
{   
    uint32_t ack = hdr->th_ack;

    if (ack < tcb->snd_una)
    {
        LOG_WARN("[handle_ack] Ignoring stale ACK (ACK too small). Expected ACK=%u, received AKC=%u", tcb->snd_nxt, ack);
        return;
    }

    else if (ack > tcb->snd_nxt)
    {
        LOG_WARN("[handle_ack] Ignoring invalid future ACK (too large). Expected ACK=%u, received ACK=%u", tcb->snd_max, ack);
        return;
    }

    /* Potential duplicate ACK */
    //else if (ack == tcb->snd_una && tcb->snd_una !=tcb->snd_max)
    else if (ack == tcb->snd_una)
    {
        uint32_t old_wnd = tcb->snd_wnd;

        if (hdr->th_win > old_wnd)
        {
            LOG_INFO("[handle_ack] WINDOW UPDATE: snd_wnd increased from %u to %u", tcb->snd_wnd, hdr->th_win);

            tcb->snd_wnd = hdr->th_win;
            pthread_cond_broadcast(&tcb->conn_cond); // Wake any app thread blocking in utcp_send waiting for window space
        }

        if (
            data_len == 0 && // empty payload
            hdr->th_win == old_wnd &&  //snd_wnd hasn't been updated
            tcb->snd_una != tcb->snd_max // there's data in flight
        )
        {
            tcb->dupacks++;
            LOG_WARN("[handle_ack] Received duplicate ACK for seq=%u, snd_max=%u. Total count: %u", tcb->snd_una, tcb->snd_max, tcb->dupacks);
        }

        /* Three duplicate ACKs detected; handle according to the current CA algorithm. */
        if (tcb->dupacks == 3)
        {
            /* both Tahoe and RENO use fast retransmit, then set ssthresh to 50% of cwnd */
            LOG_WARN("[handle_ack] 3 Duplicate ACKs. Retransmitting sequence: %u and applying TCP %s for congestion avoidance", tcb->snd_una, CA_ALGO);

            retransmit_data(tcb, tcb->snd_una);
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
                    LOG_DEBUG("[handle_ack] RENO: Entered Fast Recovery. cwnd set to %u", tcb->snd_cwnd);
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
        size_t newly_acked_bytes = (size_t)(ack - tcb->snd_una);
        LOG_INFO("[handle_ack] Received valid ACK packet. Advancing snd_una from %u to %u (ACKed %u bytes)", tcb->snd_una, ack, newly_acked_bytes);

        tcb->dupacks = 0; // reset counter

        tcb->snd_una = ack; // update new oldest unACKed number
        
        uint32_t old_tx_head = tcb->tx_head;
        tcb->tx_head = tcb->tx_head + newly_acked_bytes;
        //tcb->tx_head = (tcb->tx_head + newly_acked_bytes) % BUF_SIZE;
        tcb->snd_wnd = hdr->th_win;

        LOG_DEBUG("[handle_ack] Window Update: send_buf_head %u -> %u, snd_wnd set to %u", old_tx_head, tcb->tx_head,
                    tcb->snd_wnd);

        /**
         * Deliver bytes from the TX buffer to the app, then tell the sleeping app's
         * thread that there is now room in the TX, so it can wake up (in utcp_send())
         * and send more data.
         */
        pthread_cond_broadcast(&tcb->conn_cond);

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

        /* Pause or reset the timer */
        if (tcb->snd_una == tcb->snd_max)
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

