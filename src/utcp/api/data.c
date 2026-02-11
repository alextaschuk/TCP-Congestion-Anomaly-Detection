/*
Logic related to sending, receiving, managing,
handling, etc. datagrams and the data inside of them
*/
#include <utcp/api/data.h>

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include <utcp/api/api.h>
#include <utcp/api/globals.h>

#include <tcp/tcp_segment.h>

#include <utils/err.h>
#include <utils/printable.h>

int send_dgram(int sock, int utcp_fd, void *buf, size_t len, int flags)
{
    tcb_t *tcb = get_tcb(utcp_fd);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(tcb->dest_udp_port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // allocate for a segment (TCP header + data)
    size_t segment_size = sizeof(struct tcphdr) + len;
    tcp_segment *segment = malloc(segment_size);
    if (!segment)
        err_sys("[send_dgram]error allocating segment");
    
    memset(&segment->hdr, 0, sizeof(struct tcphdr));
    segment->hdr.th_sport = htons(tcb->fourtuple.source_port);
    segment->hdr.th_dport = htons(tcb->fourtuple.dest_port);
    segment->hdr.th_seq = htonl(tcb->snd_nxt);
    segment->hdr.th_ack = htonl(tcb->rcv_nxt);
    segment->hdr.th_off = sizeof(struct tcphdr) / 4; // Convert into 32-bit words
    segment->hdr.th_flags = flags;
    segment->hdr.th_win = htons(tcb->rcv_wnd); // dummy window

    // add buffer to the payload
    memcpy(segment->data, buf, len);

    // send the datagram
    size_t dgram_len = sizeof(struct tcphdr);
    ssize_t bytes_sent = sendto(sock, segment, segment_size, 0, (struct sockaddr*)&addr, sizeof(addr));

    if (bytes_sent < 0)
        err_sys("[send_dgram]error sending packet");
    
    printf("[send_dgram]Sending datagram to UTCP port %u, UDP port %u\r\n", tcb->fourtuple.dest_port, tcb->dest_udp_port);
    printf("Content of TCP header:\n");
    print_tcphdr(&segment->hdr);
    
    free(segment);
    tcb->snd_nxt += len;
    return bytes_sent;
}

ssize_t rcv_dgram(int sock, uint8_t *rcvbuf, ssize_t bufsize, struct sockaddr_in* from)
{

    ssize_t rcvsize; // # bytes rcv'd
    socklen_t fromlen = sizeof(*from); //# of addr bytes written (should be 16)

    rcvsize = recvfrom(sock, rcvbuf, bufsize, 0, (struct sockaddr *)from, &fromlen);
    
    if (rcvsize < 0)
        err_sys("[rcv_dgram]Failed to receive datagram");
    if (rcvsize == 0)
        printf("TODO handle connection shutdown process\n");
    return rcvsize;
}


ssize_t utcp_send(int utcp_fd, const void *buf, size_t len)
{
    tcb_t *tcb = get_tcb(utcp_fd);

    size_t free = ring_buf_free(&tcb->tx_buf);
    if (free == 0)
        return 0;  // send buffer full

    if (len > free)
        len = free; // write the max amount of data possible

    ring_buf_write(&tcb->tx_buf, buf, len);
    
    return len;
}



ssize_t utcp_recv(int utcp_fd, void *buf, size_t len)
{
    tcb_t *tcb = get_tcb(utcp_fd);

    size_t used = ring_buf_used(&tcb->rx_buf);
    if (used == 0)
        return 0; // no data available

    if (len > used)
        len = used; // read the max data amount of data possible

    ring_buf_read(&tcb->rx_buf, buf, len);

    // Update window after app consumes data
    tcb->rcv_wnd = ring_buf_free(&tcb->rx_buf);

    return len;
}