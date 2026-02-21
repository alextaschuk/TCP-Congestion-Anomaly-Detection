#include <utcp/api/tx_dgram.h>

#include <arpa/inet.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <tcp/tcp_segment.h>
#include <utils/printable.h>
#include <utils/err.h>

int send_dgram(int sock, tcb_t *tcb, void *buf, size_t payload_len, int flags)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(tcb->dest_udp_port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // hardcoded for now

    // allocate for a segment (TCP header + payload)
    size_t segment_size = sizeof(struct tcphdr) + payload_len;
    tcp_segment_t *segment = malloc(segment_size);
    if (!segment)
        err_sys("[send_dgram] error allocating segment\n");
    
    // initialize the segment's header
    memset(&segment->hdr, 0, sizeof(struct tcphdr));
    segment->hdr.th_sport = htons(tcb->fourtuple.source_port);
    segment->hdr.th_dport = htons(tcb->fourtuple.dest_port);
    segment->hdr.th_seq = htonl(tcb->snd_nxt);
    segment->hdr.th_ack = htonl(tcb->rcv_nxt);
    segment->hdr.th_off = sizeof(struct tcphdr) / 4; // Convert into 32-bit words
    segment->hdr.th_flags = flags;
    segment->hdr.th_win = htons(tcb->rcv_wnd);

    if (payload_len > 0) // add buffer to the payload
        memcpy(segment->data, buf, payload_len);

    // send the datagram
    ssize_t bytes_sent = sendto(sock, segment, segment_size, 0, (struct sockaddr*)&addr, sizeof(addr));
    if (bytes_sent < 0)
        err_sys("[send_dgram] error sending packet\n");
    
    printf("[send_dgram] Sending datagram to UTCP port %u, UDP port %u\n",  tcb->fourtuple.dest_port, tcb->dest_udp_port);
    print_segment((u_int8_t *)segment, segment_size, 0);

    tcb->snd_nxt += payload_len;

    if (flags & TH_SYN)
        tcb->snd_nxt += 1;

    free(segment);
    return bytes_sent;
}

int send_ack(tcb_t *tcb, int udp_sock, struct tcphdr *hdr, ssize_t payload_len)
{
    (void)payload_len;

    uint32_t ack = hdr->th_ack;

    if (ack < tcb->snd_una || ack > tcb->snd_nxt)
    {
        printf("[send_ack] Ignoring invalid ACK %u (snd_una=%u, snd_nxt=%u)\n", ack, tcb->snd_una, tcb->snd_nxt);
        return 0;
    }

    uint32_t acked_bytes = ack - tcb->snd_una;
    tcb->snd_una = ack;
    tcb->snd_wnd = hdr->th_win;

    ring_buf_read(&tcb->tx_buf, NULL, acked_bytes); // remove acked bytes

    // send cumulative ACK using current rcv_nxt
    send_dgram(udp_sock, tcb, NULL, 0, TH_ACK);
    printf("[send_ack] Acknowledged %u bytes\n", acked_bytes);
    return acked_bytes;
}
