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

int send_dgram(int sock, tcb_t *tcb, void *buf, size_t len, int flags)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(tcb->dest_udp_port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // allocate for a segment (TCP header + data)
    size_t segment_size = sizeof(struct tcphdr) + len;
    tcp_segment *segment = malloc(segment_size);
    if (!segment)
        err_sys("[send_dgram] error allocating segment\n");
    
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
        err_sys("[send_dgram] error sending packet\n");
    
    printf("[send_dgram] Sending datagram to UTCP port %u, UDP port %u\n",  tcb->fourtuple.dest_port, tcb->dest_udp_port);
    print_segment((u_int8_t *)segment, segment_size, 0);

    tcb->snd_nxt += len;
    tcb->snd_una += len;
    if (flags & TH_SYN)
        tcb->snd_nxt += 1;

    free(segment);
    return bytes_sent;
}


ssize_t rcv_dgram(int sock, tcb_t *tcb, ssize_t buflen)
{
    api_t *global = api_instance();
    ssize_t rcvsize; // # bytes rcv'd
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from); // # of addr bytes written (should be 16)
    uint8_t *buf = NULL;

    //tcp_segment *segment = (tcp_segment *)(void *)buf;
    struct tcphdr *hdr;
    uint8_t* data; // should be empty
    ssize_t data_len; // should be 0

    if (ring_buf_free(&tcb->rx_buf) == 0)
        return -1; // buffer is full, ignore incoming data

    if (buflen <= 0)
        return -1;

    buf = malloc((size_t)buflen);
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
        err_sys("[rcv_dgram] header dest port doesn't match tcb src port");

    switch (tcb->fsm_state)
    {
        case LISTEN: // received a SYN packet
            printf("[Server] Received SYN\n");
            if ((hdr->th_flags & TH_SYN) != 0)
            {
                if (data_len > 0)
                    err_data("[rcv_dgram] A SYN packet contains non-header data");

                // initialize TCB (TODO: move to separate function?)
                tcb->fourtuple.dest_port = hdr->th_sport; // UTCP port
                tcb->fourtuple.dest_ip = ntohl(from.sin_addr.s_addr);
                tcb->dest_udp_port = ntohs(from.sin_port);
                
                tcb->fsm_state = SYN_RECEIVED;
                tcb->irs = hdr->th_seq;
                tcb->rcv_nxt = tcb->irs + 1; // Increase sequence number by one
                tcb->rcv_wnd = BUF_SIZE;

                // send a SYN-ACK response
                printf("[Server] Sending SYN-ACK\n");
                tcb->iss = 0;
                tcb->snd_una = tcb->iss;
                tcb->snd_nxt = tcb->iss;
                send_dgram(sock, tcb, NULL, 0, TH_SYN | TH_ACK);
            } else
                err_data("[rcv_dgram] Expected SYN flag, but none was found");
            break;

        case SYN_SENT: // received a SYN-ACK packet
            if (hdr->th_flags == (TH_SYN | TH_ACK))
            {
                printf("[Client] Received SYN-ACK\n"); 
                if (data_len > 0) // we won't be using TCP Fast Open
                    err_data("[rcv_syn_ack] ACK contains non-header data");
                if (hdr->th_ack != tcb->snd_nxt)
                    err_data("[rcv_syn_ack] Received ACK # is not equal to snd_nxt value");
            
                tcb->irs = hdr->th_seq;
                tcb->rcv_nxt = hdr->th_seq + 1;
                tcb-> snd_una = hdr->th_ack;

                // send an ACK response
                // for now, we won't let the app add data to the final ACK
                send_dgram(sock, tcb, NULL, 0, TH_ACK);
                tcb->fsm_state = ESTABLISHED;
            } else
                err_data("[rcv_syn_ack] Missing SYN and/or ACK flag(s) for SYN-ACK");
            break;

        case SYN_RECEIVED: // received an ACK packet to complete 3WHS
            if (hdr->th_flags & TH_ACK)
            {
                printf("[rcv_dgram] Server Received ACK, connection established.\n");
                if (data_len > 0) // TODO: Logic to allow data in ACK
                    err_data("[rcv_dgram] An ACK packet contains non-header data\n");
                if (hdr->th_ack != tcb->snd_nxt)
                    err_data("[rcv_dgram] ACK header's th_ack is not equal to TCB's snd_nxt\n");
                
                tcb->fsm_state = ESTABLISHED;
                tcb->snd_una = hdr->th_ack;
            } else
                err_data("[rcv_dgram] Expected ACK flag, but none was found");
            break;

        case ESTABLISHED: // 3WHS is complete; received packet containing data
            if (hdr->th_flags & TH_ACK)
            {
                if (hdr->th_ack == tcb->snd_una)
                    break; // dupe ACK. TODO: track number of dupe ACKs for congestion control
                if (hdr->th_ack > tcb->snd_una && hdr->th_ack <= tcb->snd_nxt)
                    {
                        tcb->snd_una -= hdr->th_ack;
                        ring_buf_read(&tcb->tx_buf, NULL, hdr->th_ack);
                        tcb->snd_wnd = hdr->th_win;
                    }
            }
            if (hdr->th_seq != tcb->rcv_nxt)
                break; //ignore out of order data (for now)
            if((size_t)buflen > ring_buf_free(&tcb->rx_buf))
                break; // can't fit all data in the buffer, ignore (for now)
            
            ring_buf_write(&tcb->rx_buf, data, data_len);
            tcb->rcv_nxt += data_len;
            tcb->rcv_wnd = ring_buf_free(&tcb->rx_buf);
            send_dgram(sock, tcb, NULL, 0, TH_ACK);
    }


    free(buf);

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
