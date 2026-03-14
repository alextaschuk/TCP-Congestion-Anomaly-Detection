#include <utcp/rx/handle_data.h>

#include <stdio.h>

#include <tcp/congestion_control.h>
#include <utils/logger.h>
#include <utcp/api/globals.h>
#include <utcp/api/tx_dgram.h>
#include <utcp/api/utcp_timers.h>
#include <utcp/rx/handle_tcp_options.h>


void handle_data(
    tcb_t *tcb,
    int udp_fd,
    struct tcphdr *hdr,
    uint8_t *data,
    ssize_t data_len
)
{
    uint32_t ack = hdr->th_ack;

    process_tcp_options(tcb, hdr, false);

    if
    (
        SEQ_GT(ack, tcb->snd_una) && // Ensure packet isn't ACKing bytes that were already ACKed
        SEQ_LEQ(ack, tcb->snd_max)   // Ensure packet isn't ACKing unsent butes
    )
    {   /* Valid ACK! */
        uint32_t newly_acked_bytes = ack - tcb->snd_una;
        LOG_INFO("[handle_data] VALID ACK: Advancing snd_una from %u to %u (ACKed %u bytes)", tcb->snd_una,
                    ack, newly_acked_bytes);

        tcb->snd_una = ack; // update new oldest unACKed byte

        // Prevent snd_nxt from falling behind snd_una during recovery
        if (SEQ_GT(tcb->snd_una, tcb->snd_nxt))
            tcb->snd_nxt = tcb->snd_una;

        tcb->dupacks = 0; // clear the duplicate ACK counter

        // slide the snd_wnd over
        uint32_t old_tx_head = tcb->tx_head;
        tcb->tx_head = tcb->tx_head + newly_acked_bytes;
        tcb->snd_wnd = GET_SCALED_WIN(tcb, hdr);

        LOG_DEBUG("[handle_data] SND_WND UPDATE: tx_head %u -> %u, snd_wnd set to %u. Waking any blocking app threads.", old_tx_head, tcb->tx_head, tcb->snd_wnd);
        pthread_cond_broadcast(&tcb->conn_cond); // wake up blocking thread in utcp_send

        // handle the retransmission timer (pause or reset)
        if (tcb->snd_una == tcb->snd_max)
        {
            LOG_DEBUG("[handle_data] All in-flight data has been ACKed. Pausing the REXMT timer.");
            pause_timer(tcb, TCPT_REXMT); 
        }
        else
        {
            LOG_DEBUG("[handle_data] Data is still in flight. Restarting the timer.");
            //reset_timer(tcb, TCPT_REXMT);
            int rearm_ticks = tcb->rxtcur;
            int backoff = 1;
            if (tcb->rxtshift > 0)
            {
                backoff = tcp_backoff[MAXRXTSHIFT];
                rearm_ticks = tcb->rxtcur * backoff;
                if (rearm_ticks > TCPTV_REXMTMAX)
                    rearm_ticks = TCPTV_REXMTMAX;
            }
            tcb->t_timer[TCPT_REXMT] = rearm_ticks;
        }

        if (tcb->cc && tcb->cc->ack_received)
        {
            tcb->cc->ack_received(tcb, newly_acked_bytes);
        }
        else LOG_ERROR("[handle_data] Missing CC handler and/or valid ACK handler");
    }
    else if (ack == tcb-> snd_una)
    { 
        uint32_t current_scaled_win = GET_SCALED_WIN(tcb, hdr);

        if (current_scaled_win > tcb->snd_wnd)
        {
            LOG_INFO("[handle_data] WINDOW UPDATE: snd_wnd increased from %u to %u", tcb->snd_wnd, current_scaled_win);
            tcb->snd_wnd = current_scaled_win;

            pthread_cond_broadcast(&tcb->conn_cond); // notify app thread that is blocking in utcp_send
        }
        else if 
        (
            data_len == 0 && // payload is empty
            current_scaled_win == tcb->snd_wnd && // packet didn't update send window
            tcb->snd_una != tcb->snd_max // there is data in flight
        )
        { /* Possible duplicate ACK */
            tcb->snd_wnd = current_scaled_win; // Track shrinking window so future comparisons stay accurate

            if(tcb->dupacks < 255)
                tcb->dupacks++; // prevent overflow

            LOG_WARN("[handle_data] DUPLICATE ACK: recieved a duplicate ACK for seq=%u (Count: %u). snd_max=%u", tcb->snd_una,
                        tcb->dupacks, tcb->snd_max);
            
            if (tcb->cc && tcb->cc->duplicate_ack)
            {
                tcb->cc->duplicate_ack(tcb);
            }                
        //if (tcb->dupacks == 3)
        //      { // handle triple ACK according to current CA algo
        //        // both Tahoe and RENO use fast retransmit, then set ssthresh to 50% of cwnd.
        //        uint32_t old_ssthresh = tcb->ssthresh;
        //        uint32_t flight_size = tcb->snd_nxt - tcb->snd_una;
        //        LOG_WARN("[handle_data] (%d) handling triple ACK. flight_size=%u", CC_ALGO, flight_size);
        //        
        //        tcb->ssthresh = calc_ssthresh(flight_size);
        //        
        //        LOG_INFO("[handle_data] Fast Retransmit: ssthresh dropped %u -> %u", old_ssthresh, tcb->ssthresh);
        //
        //        switch (CC_ALGO)
        //        {
        //            case (TAHOE):
        //                LOG_DEBUG("[handle_data] TAHOE: Treating triple ACK as timeout. flight_size=%u", flight_size);
        //                tcb->cwnd = MSS; // drop to 1 MSS b/c of "timeout"
        //                retransmit_data(tcb, tcb->snd_una);
        //                break;
        //
        //            case (RENO):
        //                /**
        //                 * Only enter Fast Retransmit if snd_una is larger than
        //                 * the previous recovery point. This ensures that we don't
        //                 * re-enter recovery for the same window after a partial-ACK.
        //                 */
        //                if (SEQ_GT(tcb->snd_una, tcb->recover))
        //                {
        //                    tcb->recover = tcb->snd_nxt;
        //                    retransmit_data(tcb, tcb->snd_una);
        //
        //                    tcb->cwnd = tcb->ssthresh + (3 * MSS); // inflate window by 3 MSS for 3 unACKed packets
        //                    tcb->fast_recovery = true;
        //                    LOG_WARN("RENO Fast Retransmit/Recovery: flight_size=%u, ssthresh=%u, inflated cwnd=%u", flight_size,
        //                                tcb->ssthresh, tcb->cwnd);
        //
        //                    const char *old_category = current_thread_cat;
        //                    current_thread_cat = "cc_data";
        //                    LOG_INFO("TRIPLE_ACK,%u,%u", tcb->cwnd, tcb->ssthresh);
        //                    current_thread_cat = old_category;
        //
        //                    //tcb->snd_nxt = tcb->snd_una; // rewind back to the last unacket packet and resend the window
        //                    retransmit_data(tcb, tcb->snd_una);
        //                    send_dgram(tcb);
        //                    break;
        //                }
        //                else
        //                {
        //                    LOG_WARN("[handle_data] 3 duplicate ACKs but snd_una=%u <= recover=%u."
        //                                "Skipping fast retransmit.", tcb->snd_una, tcb->recover);
        //                } 
        //            default:
        //                LOG_FATAL("[handle_data] HEY! Invalid congestion avoidance algorithm.");
        //                break;
        //        }
        //    }
        //    else if (tcb->dupacks > 3 && CC_ALGO == RENO && tcb->fast_recovery)
        //    {
        //        tcb->cwnd += MSS;
        //        LOG_DEBUG("[handle_data] RENO Fast Recovery: inflating cwnd to %u", tcb->cwnd);
        //        send_dgram(tcb); // continue trying to send data
        //    }
        }
    }

    // Recieve window: Handle my acknowledgment
    if (data_len <= 0)
    { // Output data for new segments if not new data to handle.
        send_dgram(tcb);
        return;
    }

    uint32_t seq_num = hdr->th_seq;
    LOG_DEBUG("[handle_data] Processing Payload: seq_num=%u, length=%zd, expecting rcv_nxt=%u", seq_num, data_len, tcb->rcv_nxt);

    /**
     * Sequence number is past out expectation. The payload contains data
     * that we have already placed in the RX buffer (in order), but it might
     * contain new data too.
     */
    if (SEQ_LT(seq_num, tcb->rcv_nxt))
    {
        uint32_t duplicate_bytes = tcb->rcv_nxt - seq_num;

        if (duplicate_bytes >= data_len)
        { // the entire payload is already in the RX buffer (it's duplicate data)
            LOG_WARN("[handle_data] DROPPING DUPLICATE PAYLOAD: seq=%u, (len=%zd) is strictly before rcv_nxt=%u. Forcing ACK", seq_num,
                        data_len, tcb->rcv_nxt);
            
            tcb->t_flags |= F_ACKNOW;
            send_dgram(tcb);
            return;
        }

        LOG_INFO("[handle_data] OVERLAPPING PAYLOAD: Trimming first %u duplicate bytes from the payload. seq_num: %u -> %u, \
            data_len: %zd -> %zd", duplicate_bytes, seq_num, seq_num + duplicate_bytes, data_len, data_len - duplicate_bytes);
        
        seq_num += duplicate_bytes;
        data += duplicate_bytes;
        data_len -= duplicate_bytes;
    }

    if(seq_num == tcb->rcv_nxt)
    { // is this the packet we're expecting?
        uint32_t rx_free_space = BUF_SIZE - (tcb->rx_tail - tcb->rx_head);
        LOG_DEBUG("[handle_data] Buffer Check: free space=%u, incoming data=%zd", rx_free_space, data_len);

        if (data_len <= (ssize_t)rx_free_space)
        { // copy new data from payload into RX byte-by-byte
            uint32_t old_tail = tcb->rx_tail;

            for(ssize_t i = 0; i < data_len; i++)
            { // TODO: replace with memcpy
                tcb->rx_buf[(tcb->rx_tail + i) % BUF_SIZE] = data[i];
            }

            tcb->rx_tail += data_len;
            tcb->rcv_nxt += data_len;

            LOG_DEBUG("[handle_data] IN-ORDER DATA ACCEPTED: recv_buf_tail %u -> %u, rcv_nxt %u -> %u. Waking any blocking app threads.",
                    old_tail, tcb->rx_tail, (uint32_t)(tcb->rcv_nxt - data_len), tcb->rcv_nxt);

            pthread_cond_broadcast(&tcb->conn_cond); // wake app thread that is blocking in utcp_recv

            tcb->t_flags |= F_ACKNOW;
        }
        else
        {
            LOG_ERROR("[handle_data] DROP PAYLOAD: RX Buffer is full. Free space=%u, tried to insert %zd bytes", rx_free_space, data_len);
            return;
        }
    }
    else
    {
        LOG_WARN("[handle_data] DATA OUT OF ORDER: Expected %u, got %u. Dropping packet and forcing ACK.", tcb->rcv_nxt, hdr->th_seq);
        tcb->t_flags |= F_ACKNOW;
    }

    send_dgram(tcb); // ACK the received packet
}
