#include <utcp/api/tx_dgram.h>

#include <arpa/inet.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <tcp/tcp_segment.h>
#include <utils/printable.h>
#include <utils/err.h>
#include <utcp/api/globals.h>

int send_dgram(
    tcb_t *snder_tcb,
    int snder_sock,
    void *buf,
    size_t payload_len,
    int flags
)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(snder_tcb->dest_udp_port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // hardcoded for now

    /**
     * Option length for timestamps in bytes. 10 bytes for the timestamp & 8 bits bytes for NOP padding
     * 
     * We need 2 bytes of padding because the total size of the timestamp + header is 30 bytes (w/out)
     * padding, which is not divisible by 4. So the options part of the segment looks like this:
     * NOP, NOP, Kind, Length, TSval, TSecr.
     */
    size_t opt_len = 12;

    /* Allocate for a segment (TCP header + options + payload) */
    size_t segment_size = sizeof(struct tcphdr) + opt_len + payload_len;
    tcp_segment_t *segment = malloc(segment_size);
    if (!segment)
        err_sys("[send_dgram] error allocating segment\n");
    
    /* Initialize the segment's base header */
    memset(&segment->hdr, 0, sizeof(struct tcphdr));
    segment->hdr.th_sport = htons(snder_tcb->fourtuple.source_port);
    segment->hdr.th_dport = htons(snder_tcb->fourtuple.dest_port);
    segment->hdr.th_seq = htonl(snder_tcb->snd_nxt);
    segment->hdr.th_ack = htonl(snder_tcb->rcv_nxt);
    segment->hdr.th_off = (sizeof(struct tcphdr) + opt_len) / 4; // convert into 32-bit words
    segment->hdr.th_flags = flags;
    segment->hdr.th_win = htons(snder_tcb->rcv_wnd);

    /* Add timestamps for RTT calculation */
    uint8_t ts_opt[12];
    ts_opt[0] = 1; // NOP (TCPOPT_NOP)
    ts_opt[1] = 1; // NOP (TCPOPT_NOP)
    ts_opt[2] = 8; // Option-Kind = timestamp & previous timestamp's echo
    ts_opt[3] = 10; // Option-Length = 10 bytes
    
    uint32_t raw_val = htonl(tcp_now()); // update timestamp
    uint32_t raw_ecr = htonl(snder_tcb->ts_rcv_val); // echo the peer's TSval

    memcpy(&ts_opt[4], &raw_val, 4);
    memcpy(&ts_opt[8], &raw_ecr, 4);
    memcpy(segment->data, ts_opt, opt_len);

    /* Add buffer to the payload and send the segment */
    if (payload_len > 0)
        memcpy(segment->data + opt_len, buf, payload_len); // offset pointer by opt_len so that timestamps arent overwritten

    ssize_t bytes_sent = sendto(snder_sock, segment, segment_size, 0, (struct sockaddr*)&addr, sizeof(addr));
    if (bytes_sent < 0)
        err_sys("[send_dgram] error sending packet\n");
    
    printf("[send_dgram] Sending datagram to UTCP port %u, UDP port %u\n",  snder_tcb->fourtuple.dest_port, snder_tcb->dest_udp_port);
    print_segment((u_int8_t *)segment, segment_size, 0);

    snder_tcb->snd_nxt += payload_len;

    if (flags & TH_SYN)
        snder_tcb->snd_nxt += 1;

    free(segment);
    return bytes_sent;
}

