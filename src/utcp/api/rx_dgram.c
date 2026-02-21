#include <utcp/api/rx_dgram.h>

#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <utcp/api/api.h>
#include <utcp/api/globals.h>
#include <utcp/api/tx_dgram.h>
#include <utcp/api/conn.h>
#include <tcp/tcb_queue.h>
#include <utcp/api/conn.h>

ssize_t rcv_dgram(int udp_fd, ssize_t buflen)
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

    rcvsize = recvfrom(udp_fd, buf, (size_t)buflen, 0, (struct sockaddr *)&from, &fromlen);

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

    // demux the packet
    deserialize_utcp_packet(buf, rcvsize, &hdr, &data, &data_len);

    uint16_t local_utcp_port = hdr->th_dport;
    uint32_t remote_ip = ntohl(from.sin_addr.s_addr);
    uint16_t remote_udp_port = ntohs(from.sin_port);

    tcb_t *target_tcb = demux_tcb(global, local_utcp_port, remote_ip, remote_udp_port);

    if (target_tcb == NULL)
    {
        printf("[rcv_dgram] No matching TCB found for port %u. Dropping the packet.\n", local_utcp_port);
        free(buf);
        return rcvsize;
    }

    target_tcb->snd_wnd = hdr->th_win;

    switch (target_tcb->fsm_state)
    {
        case LISTEN:
            if ((hdr->th_flags & TH_SYN) != 0)
                rcv_syn(target_tcb, udp_fd, hdr, data_len, from);
            else
                err_data("[rcv_syn] Expected SYN flag, but none was found");
            break;
        case SYN_SENT:
            if ((hdr->th_flags & TH_SYN) && (hdr->th_flags & TH_ACK))
                rcv_syn_ack(target_tcb, udp_fd, hdr, data_len);
            else
                err_data("[rcv_syn_ack] Missing SYN and/or ACK flag(s) for SYN-ACK");
            break;
        case SYN_RECEIVED:
            if (hdr->th_flags & TH_ACK)
                rcv_ack(target_tcb, hdr, data_len, from);
            else
                err_data("[rcv_ack] Expected ACK flag, but none was found");
            break;
        case ESTABLISHED: // 3WHS is complete; received packet containing data
            handle_data(target_tcb, udp_fd, hdr, data, data_len, buflen);
            break;
        default:
            err_sys("[rcv_dgram] TCB's FSM is not in a valid state to receive data");
    }

    free(buf);
    return rcvsize;
}


static void rcv_syn(
    tcb_t *listen_tcb,
    int udp_sock,
    struct tcphdr *hdr,
    ssize_t data_len,
    struct sockaddr_in from
)
{
        if (data_len > 0)
            err_data("[rcv_syn] SYN packet contains non-header data in its payload");

        printf("[rcv_syn] Received valid SYN. Spawning new TCB.\n");

        struct sockaddr_in server = {
        .sin_family = AF_INET,
        .sin_port = htons(listen_tcb->fourtuple.source_port),
        .sin_addr.s_addr = inet_addr("127.0.0.1"),
        };

        uint32_t dest_ip = ntohl(from.sin_addr.s_addr);
        uint16_t dest_utcp_port =  hdr->th_sport;
        uint16_t dest_udp_port = ntohs(from.sin_port);
        uint16_t src_utcp_port = listen_tcb->fourtuple.source_port;
        
        tcb_t *new_tcb = alloc_new_tcb();

        // would be better to write this check in its own function so that it doesn't remove the existing
        // TCB from the SYN queue if it passes
        pthread_mutex_lock(&listen_tcb->syn_q.lock);
        if (remove_from_syn_queue(&listen_tcb->syn_q, dest_ip, dest_utcp_port) != NULL)
        {
            printf("Incoming request is already in SYN queue; ignoring this packet\n");
            return;
        }
        pthread_mutex_unlock(&listen_tcb->syn_q.lock);

        // configure the new connection's TCB
        new_tcb->fourtuple.dest_port = dest_utcp_port;
        new_tcb->fourtuple.dest_ip = dest_ip;
        new_tcb->dest_udp_port = dest_udp_port;
        new_tcb->fourtuple.source_port = src_utcp_port;

        new_tcb->fsm_state = SYN_RECEIVED;
        new_tcb->irs = hdr->th_seq;
        new_tcb->rcv_nxt = new_tcb->irs + 1;
        new_tcb->rcv_wnd = BUF_SIZE;

        new_tcb->iss = 500; // TODO: replace with randomly generated SEQ num
        new_tcb->snd_una = new_tcb->iss;
        new_tcb->snd_nxt = new_tcb->iss;

        printf("new TCB:\n");
        print_tcb(new_tcb);

        // add the new TCB to the SYN queue
        pthread_mutex_lock(&listen_tcb->syn_q.lock);
        enqueue_tcb(new_tcb, &listen_tcb->syn_q);
        pthread_mutex_unlock(&listen_tcb->syn_q.lock);
        
        printf("[rcv_syn] Sending SYN-ACK\n");
        send_dgram(udp_sock, new_tcb, NULL, 0, TH_SYN | TH_ACK);
}


static void rcv_syn_ack(
    tcb_t *tcb,
    int udp_sock,
    struct tcphdr *hdr,
    ssize_t data_len
)
{
        if (data_len > 0) // we won't be using TCP Fast Open
            err_data("[rcv_syn_ack] ACK contains non-header data");

        if (hdr->th_ack != tcb->snd_nxt)
            err_data("[rcv_syn_ack] Received ACK # is not equal to snd_nxt value");
        
        printf("[rcv_syn_ack] Received SYN-ACK\n");

        tcb->irs = hdr->th_seq;
        tcb->snd_una = hdr->th_ack;
        tcb->rcv_nxt = hdr->th_seq + 1;
        tcb->rcv_wnd = hdr->th_win;

        // for now, we won't let the app add data to the final ACK
        send_dgram(udp_sock, tcb, NULL, 0, TH_ACK);
        tcb->fsm_state = ESTABLISHED;
}


static void rcv_ack(tcb_t *target_tcb, struct tcphdr *hdr, ssize_t data_len, struct sockaddr_in from)
{
    if (hdr->th_ack != target_tcb->snd_nxt)
            err_data("[rcv_ack] ACK header's th_ack is not equal to TCB's snd_nxt\n");

    tcb_t *listen_tcb = find_listen_tcb();
    uint32_t remote_ip = ntohl(from.sin_addr.s_addr);
    uint16_t remote_utcp_port = hdr->th_sport;


    printf("[rcv_ack] Server Received ACK. 3WHS done.\n");

    target_tcb->fsm_state = ESTABLISHED;
    target_tcb->snd_una = hdr->th_ack;

    // 2. Remove target_tcb from SYN queue and put it in ACCEPT queue
    pthread_mutex_lock(&listen_tcb->syn_q.lock);
    remove_from_syn_queue(&listen_tcb->syn_q, target_tcb->fourtuple.dest_ip, target_tcb->dest_udp_port);
    pthread_mutex_unlock(&listen_tcb->syn_q.lock);

    pthread_mutex_lock(&listen_tcb->accept_q.lock);
    enqueue_tcb(&listen_tcb->accept_q, target_tcb);
    pthread_cond_signal(&listen_tcb->accept_q.cond); // Wake up utcp_accept!
    pthread_mutex_unlock(&listen_tcb->accept_q.lock);
        
    printf("[rcv_ack] Connection established and queued for accept().\n");
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


tcb_t* demux_tcb(api_t *global, uint16_t local_utcp_port, uint32_t remote_ip, uint16_t remote_udp_port)
{
    tcb_t *listen_tcb = NULL;

    // Step 1: Search for an exact match (Established/Active connection)
    // We also look for the listening socket during this pass to save time.
    for (int i = 0; i < MAX_UTCP_SOCKETS; i++) {
        tcb_t *tcb = global->tcb_lookup[i];
        //printf("\ncurrent TCB: ");
        //print_tcb(tcb);

        if (tcb == NULL)
            continue;

        // Save the listener if we find it, in case we need it for Step 3
        if (tcb->fourtuple.source_port == local_utcp_port && tcb->fsm_state == LISTEN)
        {
            printf("[demux_tcb] found listen tcb\n");
            listen_tcb = tcb;
            print_tcb(listen_tcb);
            continue;
        }

        if (tcb->fsm_state != ESTABLISHED)
        {
            printf("[demux_tcb] Current TCB is not in an ESTABLISHED state\n");
            continue;
        }
        printf("made it here\n");
        // Exact match check
        if (tcb->fourtuple.source_port == local_utcp_port && tcb->fourtuple.dest_ip == remote_ip && tcb->dest_udp_port == remote_udp_port && tcb->fsm_state != LISTEN)
        {
            printf("[demux_tcb] Found a TCB with an active connection\n");
            return tcb;
        }
    }
    print_tcb(listen_tcb);
    // Step 2: Check the listening socket's SYN queue (Half-open connection)
    if (listen_tcb != NULL) {
        printf("[demux_tcb] Searching for TCB in SYN queue\n");

        pthread_mutex_lock(&listen_tcb->syn_q.lock);
        
        for (int i = 0; i < listen_tcb->syn_q.count; i++) {
            int idx = (listen_tcb->syn_q.head + i) % MAX_BACKLOG;

            tcb_t *syn_tcb = &listen_tcb->syn_q.tcbs[idx];
            if (syn_tcb == NULL)
                continue;
            
            if (syn_tcb->fourtuple.dest_ip == remote_ip && syn_tcb->dest_udp_port == remote_udp_port)
            {
                printf("[demux_tcb] Found a TCB with a half-open connection\n");
                pthread_mutex_unlock(&listen_tcb->syn_q.lock);
                return syn_tcb;
            }
        }
        pthread_mutex_unlock(&listen_tcb->syn_q.lock);
        
        printf("[demux_tcb] No TCB found; will handle incoming connection (SYN) request...\n");
        print_tcb(listen_tcb);
        return listen_tcb;
    }

    // Step 4: No matching socket found. The port is closed.
    printf("[demux_tcb] No TCB found, nor listen socket found.\n");
    return NULL; 
}
