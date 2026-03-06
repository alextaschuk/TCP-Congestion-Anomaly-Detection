#include <utcp/rx/handle_ack.h>

#include <stdio.h>

#include <utils/logger.h>
#include <utcp/rx/find_timestamps.h>
#include <utcp/api/globals.h>
#include <utcp/api/tx_dgram.h>


void handle_ack(tcb_t *tcb, struct tcphdr *hdr)
{   
    uint32_t ack = hdr->th_ack;

    if (ack < tcb->snd_una)
    {
        LOG_WARN("[handle_ack] Ignoring stale ACK\n");
        return;
    }

    else if (ack > tcb->snd_nxt)
    {
        LOG_WARN("[handle_ack] Ignoring invalid future ACK (too large)\n");
        return;
    }

    /* Received a duplicate ACK; update TCB's counter for congestion avoidance (CA) and possibly enter CA. */
    else if (ack == tcb->snd_una)
    {
        LOG_WARN("[handle_ack] Received duplicate ACK\n");

        if (tcb->snd_nxt > tcb->snd_una)
        {
            tcb->dupacks++;
            LOG_INFO("[handle_ack] Duplicate ACK count: %u\n", tcb->dupacks);
        }
        
        /* three duplicate ACKs detected; handle according to the current CA algorithm. */
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
                    LOG_INFO("[handle_ack] RENO: Entered Fast Recovery. cwnd set to %u.\n", tcb->snd_cwnd);
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
                LOG_INFO("[handle_ack] Reno: Inflating cwnd to %u\n", tcb->snd_cwnd);
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
            LOG_INFO("[handle_ack] Reno: Exited Fast Recovery. cwnd deflated to %u\n", tcb->snd_cwnd);
        }
        else
        { // Standard cwnd growth logic (slow start or linear)
            if (tcb->snd_cwnd < tcb->snd_ssthresh)
            { // connection is in slow start
                tcb->snd_cwnd += MSS;
                LOG_INFO("[handle_ack] Slow Start cwnd grew to %u\n", tcb->snd_cwnd);
            }
            else // congestion avoidance: linear growth
                tcb->snd_cwnd += (MSS * MSS) / tcb->snd_cwnd;
        }

        //LOG_INFO("METRIC,%u,%u,%u\n", tcp_now(), tcb->snd_cwnd, tcb->snd_ssthresh);

        /* check for TCP Options section and if timestamps are present */
        uint32_t ts_val = 0; // Timestamp value
        uint32_t ts_ecr = 0; // Echoed timestamp value

        bool has_ts_opt = find_timestamps(hdr, &ts_val, &ts_ecr);

        /* ACK has timestamp in header, so we can update the RTO */
        if (has_ts_opt)
        {
            /**
             * An ACK may contain a payload of data that is out of order, so
             * we only want to update the peer's timestamp if it is in order.
             */
            if (hdr->th_seq <= tcb->rcv_nxt)
                tcb->ts_rcv_val = ts_val;
            
            calc_rto(tcb, ts_ecr);
        }            

        tcb->snd_una = ack;
        tcb->snd_wnd = hdr->th_win;

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
            LOG_INFO("[handle_ack] All data has been ACKed. Pausing REXMT timer.\n");
            pause_timer(tcb, TCPT_REXMT); 
        }
        else
        {
            int ticks = reset_timer(tcb, TCPT_REXMT);
            LOG_INFO("[handle_ack] UnACKed data in flight. REXMT timer reset to %d ticks.\n", ticks);
        }
    }
}

