/*
Functions related to sending, receiving, managing,
handling, etc. datagrams and the data inside of them
*/
#include <utcp/api/data.h>

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <utcp/api/globals.h>
#include <utcp/api/api.h>
#include <utils/err.h>
#include <utils/printable.h>

#include <tcp/tcp_segment.h>

int send_dgram(int sock, int utcp_fd, void* buf, size_t len, int flags)
{
    /**
     * @brief send a buffer of data to a socket.
     * 
     * @param utcp_fd UTCP socket's position in tcb_lookup
     * @return bytes_sent the number of bytes sent in the
     * datagram, or -1 if fails
     */
    struct tcb *tcb = get_tcb(utcp_fd);

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
    segment->hdr.th_off = (sizeof(struct tcphdr) / 4) << 4; // Convert into 32-bit words
    segment->hdr.th_flags = flags;
    segment->hdr.th_win = htons(1024); // dummy window

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

ssize_t rcv_dgram(int sock, uint8_t rcvbuf[1024], struct sockaddr_in* from)
{
    /**
     * @brief receive a datagram
     */
    //struct sockaddr_storage from;
    ssize_t rcvsize;
    ssize_t buflen = 1500;
    socklen_t fromlen = sizeof(*from);
    rcvsize = recvfrom(sock, rcvbuf, buflen, 0, (struct sockaddr *)from, &fromlen); // # bytes rcv'd

    if (rcvsize < 0)
        err_sys("[rcv_dgram]Failed to receive datagram");
    return rcvsize;
}
