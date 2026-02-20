#include <utcp/api/rx_dgram.h>

#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <utcp/api/api.h>
#include <utcp/api/globals.h>
#include <utcp/api/tx_dgram.h>

ssize_t rcv_dgram(int sock, tcb_t *tcb, ssize_t buflen)
{
    api_t *global = api_instance();
    ssize_t rcvsize; // number of bytes rcv'd
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from); // # of addr bytes written (should be 16)
    uint8_t *buf = NULL;

    // tcp packet stuff
    struct tcphdr *hdr;
    uint8_t* data; // should be empty
    ssize_t data_len; // should be 0

    if (buflen <= 0)
        return -1;

    buf = malloc((size_t)buflen); // TODO bad for memory; improve
    if (!buf)
        err_sys("[rcv_dgram] Failed to allocate receive buffer\n");

    rcvsize = recvfrom(sock, buf, (size_t)buflen, 0, (struct sockaddr *)&from, &fromlen);

    if (rcvsize < 0)
    {
        free(buf);
        err_sys("[rcv_dgram] Failed to receive datagram\n");
    }
    if (rcvsize == 0)
    {
        free(buf);
        printf("TODO handle connection shutdown process\n");
        return rcvsize;
    }

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &from.sin_addr, ip_str, sizeof(ip_str));
    
    printf("Received a packet from %s:%d\n", ip_str, ntohs(from.sin_port));
    print_segment(buf, rcvsize, 1);

    deserialize_utcp_packet(buf, rcvsize, &hdr, &data, &data_len);

    if(hdr->th_dport != tcb->fourtuple.source_port)
        err_sys("[rcv_dgram] header's dest port doesn't match TCB src port");

    tcb->snd_wnd = hdr->th_win;

    switch (tcb->fsm_state)
    {
        case LISTEN:
            rcv_syn(tcb, sock, buf, rcvsize, hdr, data, data_len, from);
            break;
        case SYN_SENT:
            rcv_syn_ack(tcb, sock, hdr, data_len);
            break;
        case SYN_RECEIVED:
            rcv_ack(tcb, hdr, data_len);
            break;
        case ESTABLISHED: // 3WHS is complete; received packet containing data
            handle_data(tcb, sock, hdr, data, data_len, buflen);
            break;
    }

    free(buf);
    return rcvsize;
}


static void rcv_syn(
    tcb_t *tcb,
    int udp_sock,
    uint8_t *buf,
    ssize_t rcvsize,
    struct tcphdr *hdr,
    uint8_t *data,
    ssize_t data_len,
    struct sockaddr_in from
)
{
    if ((hdr->th_flags & TH_SYN) != 0)
    {
        printf("[rcv_syn] Received SYN\n");
        if (data_len > 0)
            err_data("[rcv_syn] SYN packet contains non-header data in its payload");

        tcb->fourtuple.dest_port = hdr->th_sport; // UTCP port
        tcb->fourtuple.dest_ip = ntohl(from.sin_addr.s_addr);
        tcb->dest_udp_port = ntohs(from.sin_port);

        tcb->fsm_state = SYN_RECEIVED;
        tcb->irs = hdr->th_seq;
        tcb->rcv_nxt = tcb->irs + 1;
        tcb->rcv_wnd = BUF_SIZE;

        printf("[rcv_syn] Sending SYN-ACK\n");
        tcb->iss = 500; // server's initial seq #
        tcb->snd_una = tcb->iss;
        tcb->snd_nxt = tcb->iss;
        send_dgram(udp_sock, tcb, NULL, 0, TH_SYN | TH_ACK);
    } else
        err_data("[rcv_syn] Expected SYN flag, but none was found");
}


static void rcv_syn_ack(
    tcb_t *tcb,
    int udp_sock,
    struct tcphdr *hdr,
    ssize_t data_len
)
{
    if ((hdr->th_flags & TH_SYN) && (hdr->th_flags & TH_ACK))
    {
        printf("[rcv_syn_ack] Received SYN-ACK\n");
        if (data_len > 0) // we won't be using TCP Fast Open
            err_data("[rcv_syn_ack] ACK contains non-header data");

        if (hdr->th_ack != tcb->snd_nxt)
            err_data("[rcv_syn_ack] Received ACK # is not equal to snd_nxt value");
            
        tcb->irs = hdr->th_seq;
        tcb->snd_una = hdr->th_ack;
        tcb->rcv_nxt = hdr->th_seq + 1;
        tcb->rcv_wnd = hdr->th_win;

        // for now, we won't let the app add data to the final ACK
        send_dgram(udp_sock, tcb, NULL, 0, TH_ACK);
        tcb->fsm_state = ESTABLISHED;
    } else
            err_data("[rcv_syn_ack] Missing SYN and/or ACK flag(s) for SYN-ACK");
}


static void rcv_ack(tcb_t *tcb, struct tcphdr *hdr, ssize_t data_len)
{
    if (hdr->th_flags & TH_ACK)
    {
        if (hdr->th_ack != tcb->snd_nxt)
            err_data("[rcv_ack] ACK header's th_ack is not equal to TCB's snd_nxt\n");

        printf("[rcv_ack] Server Received ACK. 3WHS done.\n");
        tcb->fsm_state = ESTABLISHED;
        tcb->snd_una = hdr->th_ack;
    } else
        err_data("[rcv_ack] Expected ACK flag, but none was found");
    //printf("\n\nSERVER POST-3WHS TCB:\n");
    //print_tcb(tcb);
}


static void handle_data(
    tcb_t *tcb,
    int udp_sock,
    struct tcphdr *hdr,
    uint8_t *data,
    ssize_t data_len,
    ssize_t buflen
)
{
    (void)buflen;

    if (hdr->th_flags & TH_ACK)
    {
        uint32_t ack = hdr->th_ack;
        if (ack <= tcb->snd_una)
            printf("[handle_data] received stale/dupe ACK\n");
        else if (ack > tcb->snd_nxt)
        {
            printf("[handle_data] Ignoring invalid future ACK\n");
        }
        else
        {
            printf("[handle_data] Received valid ACK packet\n");
            size_t acked = (size_t)(ack - tcb->snd_una);
            tcb->snd_una = ack;
            printf("ack th_win: %u\n", hdr->th_win);
            tcb->snd_wnd = hdr->th_win;
            ring_buf_read(&tcb->tx_buf, NULL, acked);
            printf("[handle_data] ACKed %zu tx bytes\n", acked);
        }
        return;
    }

    if (data_len <= 0)
        return;

    uint32_t seg_seq = hdr->th_seq;

    if (seg_seq > tcb->rcv_nxt)
    {
        printf("[handle_data] Out-of-order data (seq=%u expected=%u). Sending dup ACK.\n", seg_seq, tcb->rcv_nxt);
        send_dgram(udp_sock, tcb, NULL, 0, TH_ACK);
        return;
    }

    if (seg_seq < tcb->rcv_nxt)
    {
        uint32_t overlap = tcb->rcv_nxt - seg_seq;
        if ((size_t)overlap >= (size_t)data_len)
        {
            printf("[handle_data] Fully duplicate retransmission. Sending dup ACK.\n");
            send_dgram(udp_sock, tcb, NULL, 0, TH_ACK);
            return;
        }

        data += overlap;
        data_len -= (ssize_t)overlap;
        printf("[handle_data] Trimmed %u retransmitted bytes, accepting %zd new bytes.\n", overlap, data_len);
    }

    size_t free = ring_buf_free(&tcb->rx_buf);
    if (free == 0)
    {
        printf("[handle_data] rx buffer full, cannot accept data.\n");
        send_dgram(udp_sock, tcb, NULL, 0, TH_ACK);
        return;
    }

    if ((size_t)data_len > free)
    {
        printf("[handle_data] rx buffer limited, truncating payload from %zd to %zu bytes.\n", data_len, free);
        data_len = (ssize_t)free;
    }

    size_t written = ring_buf_write(&tcb->rx_buf, data, (size_t)data_len);
    tcb->rcv_nxt += (uint32_t)written;
    tcb->rcv_wnd = ring_buf_free(&tcb->rx_buf);

    send_dgram(udp_sock, tcb, NULL, 0, TH_ACK);
    
    printf("\n\n[handle_data] Received valid packet, sent ACK.\n\n");
    printf("Number of bytes in rx buffer: %zu\n", ring_buf_used(&tcb->rx_buf));
}
