#include <utcp/rx/handle_data.h>

#include <stdio.h>

#include <utils/logger.h>
#include <utcp/api/globals.h>
#include <utcp/api/tx_dgram.h>
#include <utcp/rx/find_timestamps.h>
#include <utcp/rx/handle_ack.h>


void handle_data(
    tcb_t *tcb,
    int udp_fd,
    struct tcphdr *hdr,
    uint8_t *data,
    ssize_t data_len
)
{
    uint32_t seg_seq = hdr->th_seq;
    uint32_t ack = hdr->th_ack;

    if
    (
        (ack - tcb->snd_una) > 0 && // is the packet ACKing bytes that were already ACKed?
        (ack - tcb->snd_max <= 0) // is the packet ACKing unsent bytes?
    )
    {
        uint32_t newly_acked_bytes = ack - tcb->snd_una;
        LOG_INFO("[handle_data] VALID ACK: Advancing snd_una from %u to %u (ACKed %u bytes)", tcb->snd_una,
                    ack, newly_acked_bytes);

        tcb->snd_una = ack; // update new oldest unACKed byte
        tcb->dupacks = 0; // clear the duplicate ACK counter

        // slide the snd_wnd over
        uint32_t old_tx_head;
        tcb->tx_head = tcb->tx_head + newly_acked_bytes;
        tcb->snd_wnd = hdr->th_win;

        LOG_DEBUG("[handle_data] SND_WND UPDATE: tx_head %u -> %u, snd_wnd set to %u. Waking any blocking app threads.", old_tx_head, tcb->tx_head, tcb->snd_wnd);
        pthread_cond_broadcast(&tcb->conn_cond);

        // handle the retransmission timer (pause or reset)
        if (tcb->snd_una == tcb->snd_max)
        {
            LOG_DEBUG("[handle_data] All in-flight data has been ACKed. Pausing the REXMT timer.");
            pause_timer(tcb, TCPT_REXMT); 
        }
        else
        {
            LOG_DEBUG("[handle_data] Data is still in flight. Restarting the timer.");
            reset_timer(tcb, TCPT_REXMT);
        }

        // if using RENO, exit fast recovery if needed
        if(CA_ALGO == RENO && tcb->fast_recovery)
        {
            tcb->snd_cwnd = tcb->snd_ssthresh; // delate artificially inflated window
            tcb->fast_recovery = false;
            LOG_INFO("[handle_data] RENO exited Fast Recovery. cwnd deflated to %u", tcb->snd_cwnd);
        }

        // congestion control & avoidance
        uint32_t old_snd_cwnd = tcb->snd_cwnd;
        if (tcb->snd_cwnd < tcb->snd_ssthresh)
        { // we are in slow start phase
            tcb->snd_cwnd += newly_acked_bytes;
            LOG_INFO("[handle_data] SLOW START: cwnd %u -> %u", old_snd_cwnd, tcb->snd_cwnd);
        }
        else
        { // we are in congestion avoidance phase (linear growth)
            tcb->snd_cwnd += (MSS * MSS) / tcb->snd_cwnd;
            LOG_DEBUG("[handle_data] CONGETSION AVOIDANCE (linear growth): cwnd %u -> %u", old_snd_cwnd, tcb->snd_cwnd);
        }

        LOG_INFO("[handle_data] ACK,%u,%u", tcb->snd_cwnd, tcb->snd_ssthresh);

    }
    else if (ack == tcb-> snd_una)
    {
        // The packet may be a duplicate ACK, but it could also be a packet 
        // to inform the receiver of a window update
        if (hdr->th_win > tcb->snd_wnd)
        {
            LOG_DEBUG("[handle_data] SND_WND UPDATE: snd_wnd increased from %u to %u. Waking any blocking app threads.", tcb->snd_wnd, hdr->th_win);
            tcb->snd_wnd = hdr->th_win;

            pthread_cond_broadcast(&tcb->conn_cond); // wake any app thread that is blocking in utcp_send for window space
        }
        
        if (
            data_len == 0 && // payload is empty
            hdr->th_win == tcb->snd_wnd && // packet didn't update send window
            tcb->snd_una != tcb->snd_max // no data is currently in flight to be ACKed
        )
        {
            tcb->dupacks++;
            LOG_WARN("[handle_data] DUPLICATE ACK: recieved a duplicate ACK for seq=%u (Count: %u). snd_max=%u", tcb->snd_una,
                        tcb->dupacks, tcb->snd_max);

            if (tcb->dupacks == 3)
            { // handle triple ACK according to current CA algo
                // both Tahoe and RENO use fast retransmit, then set snd_ssthresh to 50% of snd_cwnd.
                uint32_t old_ssthresh = tcb->snd_ssthresh;
                uint32_t flight_size = tcb->snd_nxt - tcb->snd_una;
                LOG_INFO("[handle_data] (%s) handling triple ACK. flight_size=%u", CA_ALGO, flight_size);
                
                // calculate new ssthresh
                uint32_t half_flight = flight_size / 2;
                tcb->snd_ssthresh = MAX(half_flight, 2 * MSS);
                LOG_INFO("[handle_data] Fast Retransmit: ssthresh dropped %u -> %u", old_ssthresh, tcb->snd_ssthresh);

                switch (CA_ALGO)
                {
                    case (TAHOE):
                        LOG_DEBUG("[handle_data] TAHOE: Treating triple ACK as timeout. flight_size=%u", flight_size);
                        tcb->snd_cwnd = MSS; // drop to 1 MSS b/c of "timeout"
                        retransmit_data(tcb, tcb->snd_una);
                        break;

                    case (RENO):
                        tcb->fast_recovery = true;
                        tcb->snd_cwnd = tcb->snd_ssthresh + (3 * MSS); // inflate window by 3 MSS for 3 unACKed packets
                        LOG_WARN("RENO Fast Retransmit/Recovery: flight_size=%u, ssthresh=%u, inflated cwnd=%u", flight_size,
                                    tcb->snd_ssthresh, tcb->snd_cwnd);
                        
                        tcb->snd_nxt = tcb->snd_una; // rewind back to the last unacket packet and resend the window
                        send_dgram(tcb);
                        break;
                    
                    default:
                        LOG_FATAL("[handle_data] HEY! Invalid congestion avoidance algorithm.");
                        break;
                }
            }
            else if (tcb->dupacks > 3 && CA_ALGO == RENO && tcb->fast_recovery)
            {
                tcb->snd_cwnd += MSS;
                LOG_DEBUG("[handle_data] RENO Fast Recovery: inflating cwnd to %u", tcb->snd_cwnd);
                send_dgram(tcb); // continue trying to send data
            }
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
    if (seq_num - tcb->rcv_nxt < 0)
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

        // check for TCP Options section and if timestamps are present
        uint32_t ts_val = 0; // Timestamp value
        uint32_t ts_ecr = 0; // Echoed timestamp value

        bool has_ts_opt = find_timestamps(hdr, &ts_val, &ts_ecr);

        // ACK has timestamp in header, so we can update the RTO
        if (has_ts_opt)
        {
            // An ACK may contain a payload of data that is out of order, so
            // we only want to update the peer's timestamp if it is in order.
            if (hdr->th_seq <= tcb->rcv_nxt)
            {
                tcb->ts_rcv_val = ts_val;
            }

            calc_rto(tcb, ts_ecr);
        }
        else
        {
            LOG_WARN("[handle_data] TCP header is missing timestamps. Skipping RTT update"); 
            return;
        }

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

    /*
    if (seg_seq > tcb->rcv_nxt)
    {
        LOG_WARN("[handle_data] Out-of-order data (seq=%u expected=%u). Sending duplicate ACK.\n", seg_seq, tcb->rcv_nxt);
        tcb->t_flags |= F_ACKNOW;
        send_dgram(tcb);
        return;
    }

    if (seg_seq < tcb->rcv_nxt)
    {
        uint32_t overlap = tcb->rcv_nxt - seg_seq;
        if ((size_t)overlap >= (size_t)data_len)
        {
            LOG_WARN("[handle_data] Fully duplicate transmission. Ignoring and sending a duplicate ACK.\n");
            tcb->t_flags |= F_ACKNOW;
            send_dgram(tcb);
            return;
        }

        data += overlap;
        data_len -= (ssize_t)overlap;
        LOG_INFO("[handle_data] Trimmed %u retransmitted bytes, accepting %zd new bytes.\n", overlap, data_len);
    }

    // check for TCP Options section and if timestamps are present
    uint32_t ts_val = 0; // Timestamp value
    uint32_t ts_ecr = 0; // Echoed timestamp value

    bool has_ts_opt = find_timestamps(hdr, &ts_val, &ts_ecr);

    // ACK has timestamp in header, so we can update the RTO
    if (has_ts_opt)
    {
        // An ACK may contain a payload of data that is out of order, so
        // we only want to update the peer's timestamp if it is in order.
        if (hdr->th_seq <= tcb->rcv_nxt)
        {
            tcb->ts_rcv_val = ts_val;
        }
            
        calc_rto(tcb, ts_ecr);
    }
    else
        LOG_WARN("[handle_ack] TCP header is missing timestamps. Skipping RTT update"); 

    if (hdr->th_flags & TH_ACK)
    {
        handle_ack(tcb, hdr, data_len);
    }

    if (data_len <= 0)
        return;

    uint32_t curr_buffered = tcb->rx_tail - tcb->rx_head;
    uint32_t free_space = BUF_SIZE - curr_buffered;
    if (free_space == 0)
    {
        LOG_WARN("[handle_data] rx buffer full, cannot accept data.");
        send_dgram(tcb);
        return;
    }

    if ((uint32_t)data_len > free_space)
    {
        LOG_WARN("[handle_data] rx buffer limited, truncating payload from %zd to %u bytes.", data_len, free_space);
        data_len = (ssize_t)free_space;
    }

    if (data_len <= (ssize_t)free_space)
    { // copy data into RX byte-by-byte
        uint32_t old_tail = tcb->rx_tail;

        for(ssize_t i = 0; i < data_len; i++)
        { // TODO: replace with memcpy
            tcb->rx_buf[(tcb->rx_tail + i) % BUF_SIZE] = data[i];
        }

        tcb->rx_tail += data_len;
        //tcb->rx_tail = (tcb->rx_tail + data_len) % BUF_SIZE;
        tcb->rcv_nxt += data_len;

        LOG_DEBUG("[handle_data]IN-ORDER DATA ACCEPTED: recv_buf_tail %u -> %u, rcv_nxt %u -> %u. Waking API threads.",
                old_tail, tcb->rx_tail, (uint32_t)(tcb->rcv_nxt - data_len), tcb->rcv_nxt);

        pthread_cond_broadcast(&tcb->conn_cond); // wake app thread that is blocking in utcp_recv

        tcb->t_flags |= F_ACKNOW;
    }
    else
    {
        LOG_WARN("[handle_data] DATA OUT OF ORDER: Expected %u, got %u. Dropping packet and forcing ACK.", tcb->rcv_nxt, hdr->th_seq);
    }

    send_dgram(tcb); // ACK the received packet
    */
}
