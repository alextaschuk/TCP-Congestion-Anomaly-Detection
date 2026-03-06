#include <utcp/rx/handle_data.h>

#include <stdio.h>

#include <utcp/api/globals.h>
#include <utcp/rx/handle_ack.h>
#include <utcp/api/tx_dgram.h>

#include <utils/logger.h>

void handle_data(
    tcb_t *tcb,
    int udp_fd,
    struct tcphdr *hdr,
    uint8_t *data,
    ssize_t data_len
)
{
    if (hdr->th_flags & TH_ACK)
    {
        handle_ack(tcb, hdr);
    }

    if (data_len <= 0)
        return;

    uint32_t seg_seq = hdr->th_seq;

    if (seg_seq > tcb->rcv_nxt)
    {
        LOG_WARN("[handle_data] Out-of-order data (seq=%u expected=%u). Sending dup ACK.\n", seg_seq, tcb->rcv_nxt);
        send_dgram(tcb, udp_fd, NULL, 0, TH_ACK);
        return;
    }

    if (seg_seq < tcb->rcv_nxt)
    {
        uint32_t overlap = tcb->rcv_nxt - seg_seq;
        if ((size_t)overlap >= (size_t)data_len)
        {
            LOG_WARN("[handle_data] Fully duplicate retransmission. Sending dup ACK.\n");
            send_dgram(tcb, udp_fd, NULL, 0, TH_ACK);
            return;
        }

        data += overlap;
        data_len -= (ssize_t)overlap;
        LOG_INFO("[handle_data] Trimmed %u retransmitted bytes, accepting %zd new bytes.\n", overlap, data_len);
    }

    size_t free = ring_buf_free(&tcb->rx_buf);
    if (free == 0)
    {
        LOG_WARN("[handle_data] rx buffer full, cannot accept data.\n");
        send_dgram(tcb, udp_fd, NULL, 0, TH_ACK);
        return;
    }

    if ((size_t)data_len > free)
    {
        LOG_WARN("[handle_data] rx buffer limited, truncating payload from %zd to %zu bytes.\n", data_len, free);
        data_len = (ssize_t)free;
    }

    size_t written = ring_buf_write(&tcb->rx_buf, data, (size_t)data_len);
    tcb->rcv_nxt += (uint32_t)written;
    tcb->rcv_wnd = ring_buf_free(&tcb->rx_buf);

    send_dgram(tcb, udp_fd, NULL, 0, TH_ACK); // ACK the received packet
    
    LOG_INFO("[handle_data] Received a valid packet, sent ACK.");
    LOG_INFO("Number of bytes in rx_buf: %zu\n", ring_buf_used(&tcb->rx_buf));
}
