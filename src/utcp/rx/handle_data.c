#include <utcp/rx/handle_data.h>

#include <stdio.h>

#include <tcp/congestion_control.h>
#include <tcp/ooo_buffer.h>
#include <utils/logger.h>
#include <utcp/api/api.h>
#include <utcp/api/globals.h>
#include <utcp/api/tx_dgram.h>
#include <utcp/api/utcp_timers.h>
#include <utcp/rx/handle_tcp_options.h>


void handle_data(
    tcb_t *tcb,
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
            //LOG_DEBUG("[handle_data] All in-flight data has been ACKed. Pausing the REXMT timer.");
            pause_timer(tcb, TCPT_REXMT); 
        }
        else
        {
            //LOG_DEBUG("[handle_data] Data is still in flight. Restarting the timer.");
            reset_timer(tcb, TCPT_REXMT);
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
            //LOG_INFO("[handle_data] SND_WND UPDATE: snd_wnd increased from %u to %u", tcb->snd_wnd, current_scaled_win);
            tcb->snd_wnd = current_scaled_win;
            pthread_cond_broadcast(&tcb->conn_cond); // notify app thread that is blocking in utcp_send
        }
        else if 
        (
            data_len == 0 && // payload is empty
            current_scaled_win <= tcb->snd_wnd && // packet didn't update send window
            tcb->snd_una != tcb->snd_max // there is data in flight
        )
        {
            tcb->snd_wnd = current_scaled_win; // Track shrinking window so future comparisons stay accurate

            if(tcb->dupacks < 255)
                tcb->dupacks++; // prevent overflow

            LOG_WARN("[handle_data] DUPLICATE ACK: recieved a duplicate ACK for seq=%u (Count: %u). snd_max=%u", tcb->snd_una,
                        tcb->dupacks, tcb->snd_max);
            
            if (tcb->cc && tcb->cc->duplicate_ack)
                tcb->cc->duplicate_ack(tcb);
        }
    }

    // Recieve window: Handle my acknowledgment
    if (data_len <= 0)
    { // Output data for new segments if not new data to handle.
        send_dgram(tcb);
        return;
    }

    uint32_t seq_num = hdr->th_seq;
    //LOG_DEBUG("[handle_data] Processing Payload: seq_num=%u, length=%zd, expecting rcv_nxt=%u", seq_num, data_len, tcb->rcv_nxt);

    /**
     * Sequence number is past out expectation. The payload contains data
     * that we have already placed in the RX buffer (in order), but it might
     * contain new data too.
     */
    if (SEQ_LT(seq_num, tcb->rcv_nxt))
    { // duplicate data
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
    { // in-order data (is this the packet we're expecting?)

        // We need to save room in the RX buffer for the OOO bytes that will eventually drain into it.
        uint32_t rx_free_space = BUF_SIZE - (tcb->rx_tail - tcb->rx_head) - tcb->ooo_bytes;
        LOG_DEBUG("[handle_data] Buffer Check: free space=%u, incoming data=%zd", rx_free_space, data_len);

        if (data_len <= (ssize_t)rx_free_space)
        { // copy new data from payload into RX byte-by-byte
            uint32_t old_tail = tcb->rx_tail;

            ring_buf_write(tcb->rx_buf, BUF_SIZE, tcb->rx_tail, data, data_len);

            tcb->rx_tail += data_len;
            tcb->rcv_nxt += data_len;

            LOG_DEBUG("[handle_data] IN-ORDER DATA ACCEPTED: rx_tail %u -> %u, rcv_nxt %u -> %u. Waking any blocking app threads.",
                        old_tail, tcb->rx_tail, (uint32_t)(tcb->rcv_nxt - data_len), tcb->rcv_nxt);

            pthread_cond_broadcast(&tcb->conn_cond); // wake app thread that is blocking in utcp_recv

            /* Drain any OOO segments that are now consecutive with rcv_nxt.
             * This advances rcv_nxt cumulatively — a single ACK will cover
             * all the data moved out of the reassembly queue.
             */
            uint32_t pre_drain_rcv_nxt = tcb->rcv_nxt;
            drain_ooo_queue(tcb);
            if (tcb->rcv_nxt != pre_drain_rcv_nxt) {
                LOG_INFO("[handle_data]: rcv_nxt advanced %u -> %u after hole filled.", pre_drain_rcv_nxt, tcb->rcv_nxt);
                pthread_cond_broadcast(&tcb->conn_cond); // wake app thread again sinec more data is available
            }
            if (tcb->dupacks > 0)
            {
                tcb->t_flags |= F_ACKNOW;
            }
        }
        else
        {
            LOG_ERROR("[handle_data] DROP PAYLOAD: RX Buffer is full. Free space=%u, tried to insert %zd bytes", rx_free_space, data_len);
            return;
        }
    }
    else
    { // out of order data
        /* seq_num > rcv_nxt: out-of-order segment.
         * Buffer it in the reassembly queue and send a duplicate ACK of
         * the last in-order byte (rcv_nxt) so the sender knows what we
         * are still waiting for.  The application cannot read past the
         * hole because recv_buf only contains contiguous in-order data.
         */
        LOG_WARN("[handle_data] DATA OUT OF ORDER: Expected %u, got %u. Dropping packet and forcing ACK.", tcb->rcv_nxt, hdr->th_seq);
        insert_ooo_segment(tcb, seq_num, data, (uint32_t)data_len);
        tcb->t_flags |= F_ACKNOW;
    }

    send_dgram(tcb); // ACK the received packet
}
