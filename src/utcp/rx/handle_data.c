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
}
