#include <utcp/api/rx_dgram.h>

#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>

#include <utcp/api/api.h>
#include <utcp/api/globals.h>
#include <utcp/api/tx_dgram.h>
#include <utcp/api/conn.h>
#include <tcp/tcb_queue.h>
#include <utcp/api/conn.h>
#include <utcp/api/utcp_timers.h>

ssize_t rcv_dgram(int udp_fd, ssize_t buflen)
{
    api_t *global = api_instance();
    ssize_t rcvsize; // number of bytes rcv'd
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from); // # of addr bytes written (should be 16)
    uint8_t *buf = NULL;

    // tcp packet stuff
    struct tcphdr *hdr;
    uint8_t* data;
    ssize_t data_len;

    if (buflen <= 0)
        return -1;

    buf = malloc((size_t)buflen);
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

    // deserialize & demux the packet
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
                rcv_3whs_ack(target_tcb, hdr, from);
            else
                err_data("[rcv_ack] Expected ACK flag, but none was found");
            break;

        case ESTABLISHED: // 3WHS is complete; received packet containing data
            handle_data(target_tcb, udp_fd, hdr, data, data_len);
            break;

        default:
            err_sys("[rcv_dgram] TCB's FSM is not in a valid state to receive data");
    }

    free(buf);
    return rcvsize;
}


static void rcv_syn(
    tcb_t *listen_tcb,
    int udp_fd,
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
        
        // create a new TCB for the incoming connection request
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
        new_tcb->rcv_wnd = BUF_SIZE - 1;

        new_tcb->iss = 500; // TODO: replace with randomly generated SEQ num
        new_tcb->snd_una = new_tcb->iss;
        new_tcb->snd_nxt = new_tcb->iss;

        /* Check for TCP Options section and if timestamps are present */
        uint32_t ts_val = 0; // Timestamp value in rcv'd packet
        uint32_t ts_ecr = 0; // Echoed timestamp value in rcv'd packet

        bool has_ts_opt = find_timestamps(hdr, &ts_val, &ts_ecr);

        if (has_ts_opt)
        {
            new_tcb->ts_rcv_val = ts_val;
        }

        printf("new TCB:\n");
        print_tcb(new_tcb);

        // add the new TCB to the SYN queue
        pthread_mutex_lock(&listen_tcb->syn_q.lock);
        enqueue_tcb(new_tcb, &listen_tcb->syn_q);
        pthread_mutex_unlock(&listen_tcb->syn_q.lock);
        
        printf("[rcv_syn] Sending SYN-ACK\n");
        send_dgram(new_tcb, udp_fd, NULL, 0, TH_SYN | TH_ACK);
}


static void rcv_syn_ack(
    tcb_t *tcb,
    int udp_fd,
    struct tcphdr *hdr,
    ssize_t data_len
)
{
        if (data_len > 0) // we won't be using TCP Fast Open
            err_data("[rcv_syn_ack] ACK contains non-header data");

        if (hdr->th_ack != tcb->snd_nxt)
            err_data("[rcv_syn_ack] Received ACK is not equal to snd_nxt");
        
        printf("[rcv_syn_ack] Received SYN-ACK\n");

        pthread_mutex_lock(&tcb->lock);

        tcb->irs = hdr->th_seq;
        tcb->snd_una = hdr->th_ack;
        tcb->rcv_nxt = hdr->th_seq + 1;
        tcb->rcv_wnd = hdr->th_win;

        /* check for TCP Options section and if timestamps are present */
        uint32_t ts_val = 0; // Timestamp value
        uint32_t ts_ecr = 0; // Echoed timestamp value

        bool has_ts_opt = find_timestamps(hdr, &ts_val, &ts_ecr);

        if (has_ts_opt)
        {
            tcb->ts_rcv_val = ts_val;
        }

        tcb->fsm_state = ESTABLISHED;

        pthread_cond_signal(&tcb->conn_cond); // wake up utcp_connect()
        pthread_mutex_unlock(&tcb->lock);

        // for now, we won't let the app add data to the final ACK
        send_dgram(tcb, udp_fd, NULL, 0, TH_ACK);
}


static void rcv_3whs_ack(tcb_t *target_tcb, struct tcphdr *hdr, struct sockaddr_in from)
{
    if (hdr->th_ack != target_tcb->snd_nxt)
            err_data("[rcv_ack] ACK header's th_ack is not equal to TCB's snd_nxt\n");

    tcb_t *listen_tcb = find_listen_tcb();
    uint32_t remote_ip = ntohl(from.sin_addr.s_addr);
    uint16_t remote_utcp_port = hdr->th_sport;


    printf("[rcv_ack] Server Received ACK. 3WHS done.\n");

        /* check for TCP Options section and if timestamps are present */
        uint32_t ts_val = 0; // Timestamp value
        uint32_t ts_ecr = 0; // Echoed timestamp value

        bool has_ts_opt = find_timestamps(hdr, &ts_val, &ts_ecr);

        if (has_ts_opt)
            target_tcb->ts_rcv_val = ts_val;

    target_tcb->fsm_state = ESTABLISHED;
    target_tcb->snd_una = hdr->th_ack;

    // remove target_tcb from SYN queue and put it in accept queue
    pthread_mutex_lock(&listen_tcb->syn_q.lock);
    remove_from_syn_queue(&listen_tcb->syn_q, target_tcb->fourtuple.dest_ip, target_tcb->dest_udp_port);
    pthread_mutex_unlock(&listen_tcb->syn_q.lock);

    pthread_mutex_lock(&listen_tcb->accept_q.lock);
    enqueue_tcb(target_tcb, &listen_tcb->accept_q);
    pthread_cond_signal(&listen_tcb->accept_q.cond); // Wake up utcp_accept()
    pthread_mutex_unlock(&listen_tcb->accept_q.lock);
    printf("[rcv_ack] Connection established and queued for accept().\n");
}


static void handle_data(
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
        printf("[handle_data] Out-of-order data (seq=%u expected=%u). Sending dup ACK.\n", seg_seq, tcb->rcv_nxt);
        send_dgram(tcb, udp_fd, NULL, 0, TH_ACK);
        return;
    }

    if (seg_seq < tcb->rcv_nxt)
    {
        uint32_t overlap = tcb->rcv_nxt - seg_seq;
        if ((size_t)overlap >= (size_t)data_len)
        {
            printf("[handle_data] Fully duplicate retransmission. Sending dup ACK.\n");
            send_dgram(tcb, udp_fd, NULL, 0, TH_ACK);
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
        send_dgram(tcb, udp_fd, NULL, 0, TH_ACK);
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

    send_dgram(tcb, udp_fd, NULL, 0, TH_ACK); // ACK the received packet
    
    printf("\n\n[handle_data] Received valid packet, sent ACK.\n\n");
    printf("Number of bytes in rx_buf: %zu\n", ring_buf_used(&tcb->rx_buf));
}

static void handle_ack(tcb_t *tcb, struct tcphdr *hdr)
{   
    uint32_t ack = hdr->th_ack;

    if (ack < tcb->snd_una)
    {
        printf("[handle_ack] Ignoring stale ACK\n");
        return;
    }

    else if (ack > tcb->snd_nxt)
    {
        printf("[handle_ack] Ignoring invalid future ACK (too large)\n");
        return;
    }

    /* Received a duplicate ACK; update TCB's counter for congestion avoidance (CA) and/or enter CA */
    if (ack == tcb->snd_una)
    {
        printf("[handle_ack] Received duplicate ACK\n");

        if (tcb->snd_nxt > tcb->snd_una)
        {
            tcb->dupacks++;
            printf("[handle_ack] Duplicate ACK count: %u\n", tcb->dupacks);
        }
        
        if (tcb->dupacks == 3)
        {
            printf("[handle_ack] 3 Duplicate ACKs. Retransmitting sequence: %u\n", tcb->snd_una);
            // TODO: implement congestion avoidance
        }
    }

    /* ACK is valid */
    else
    {
        printf("[handle_ack] Received valid ACK packet\n");
        size_t acked = (size_t)(ack - tcb->snd_una);

        /* check for TCP Options section and if timestamps are present */
        uint32_t ts_val = 0; // Timestamp value
        uint32_t ts_ecr = 0; // Echoed timestamp value

        bool has_ts_opt = find_timestamps(hdr, &ts_val, &ts_ecr);

        /* ACK has timestamp in header, so we can update the RTO */
        if (has_ts_opt)
        {
            tcb->ts_rcv_val = ts_val;
            calc_rto(tcb);
        }            

        if (tcb->snd_cwnd < tcb->snd_ssthresh)
        { // connection is in slow start
            tcb->snd_cwnd += MSS;
            printf("[handle_ack] Slow Start cwnd grew to %u\n", tcb->snd_cwnd);
        }

        else // congestion avoidance: linear growth
            tcb->snd_cwnd += (MSS * MSS) / tcb->snd_cwnd;

        tcb->snd_una = ack;
        tcb->snd_wnd = hdr->th_win;

        ring_buf_read(&tcb->tx_buf, NULL, acked);
    }
}


tcb_t* demux_tcb(
    api_t *global,
    uint16_t dest_utcp_port,
    uint32_t src_ip,
    uint16_t src_udp_port
)
{
    tcb_t *listen_tcb = NULL;

    /* look for a matching TCB that has an ESTABLISHED state */
    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        tcb_t *tcb = global->tcb_lookup[i];

        if (tcb == NULL)
            continue;

        uint16_t local_utcp_port = tcb->fourtuple.source_port;
        uint32_t remote_ip = tcb->fourtuple.dest_ip;
        uint16_t remote_udp_port = tcb->dest_udp_port;

        if
        ( // check 4-tuple 
            local_utcp_port == dest_utcp_port &&
            remote_ip == src_ip &&
            remote_udp_port == src_udp_port
        )
            return tcb;

        // we will return listener TCB to server if state isn't ESTABLISHED or SYN-RECEIVED
        if (local_utcp_port == dest_utcp_port && tcb->fsm_state == LISTEN)
        {
            //printf("[demux_tcb] found listen TCB\n");
            listen_tcb = tcb;
            continue;
        }
    }

    // no active connection found, look for half-open connection (3WHS is in-progress)
    if (listen_tcb != NULL)
    {
        printf("[demux_tcb] Searching for TCB in SYN queue\n");

        pthread_mutex_lock(&listen_tcb->syn_q.lock);
        for (int i = 0; i < listen_tcb->syn_q.count; i++)
        {
            int idx = (listen_tcb->syn_q.head + i) % MAX_BACKLOG;
            tcb_t *syn_tcb = &listen_tcb->syn_q.tcbs[idx];

            if (syn_tcb == NULL)
                continue;
            
            uint32_t client_ip = syn_tcb->fourtuple.dest_ip;
            uint16_t client_udp_port = syn_tcb->dest_udp_port;

            if (client_ip == src_ip && client_udp_port == src_udp_port)
            {
                printf("[demux_tcb] Found a TCB with a half-open connection\n");
                pthread_mutex_unlock(&listen_tcb->syn_q.lock);
                return syn_tcb;
            }
        }
        pthread_mutex_unlock(&listen_tcb->syn_q.lock);
        
        printf("[demux_tcb] No TCB found; will handle incoming connection (SYN) request...\n");
        return listen_tcb;
    }

    printf("[demux_tcb] No TCB found, nor listen socket found.\n");
    return NULL; 
}


static bool find_timestamps(struct tcphdr *hdr, uint32_t *ts_val, uint32_t *ts_ecr)
{
    int opt_len = (hdr->th_off * 4) - sizeof(struct tcphdr);
    uint8_t *opt_ptr = (uint8_t *)hdr + sizeof(struct tcphdr); // the 1st option byte right after the TCP header

    while (opt_len > 0) 
    { // iterate over the options array (ts_opt in send_dgram())
        uint8_t opt_kind = opt_ptr[0];

        /* check for single-byte options (only have the kind byte, no length or value bytes) */
        if (opt_kind == TCPOPT_EOL) // End of options list
            break;
        
        if (opt_kind == TCPOPT_NOP) // No operation
        {
            opt_ptr++;
            opt_len--;
            continue;
        }

        if (opt_len < 2)
            err_data("[find_timestamps] Malformed options: cutt off before length byte");

        /* check for multi-byte options that have length & value bytes*/
        uint8_t opt_size = opt_ptr[1];
        
        if (opt_size < 2 || opt_size > opt_len)
        {
            printf("[find_timestamps] opt_size: %u\n", opt_size);
            printf("[find_timestamps] opt_len: %u\n", opt_len);
            err_data("[find_timestamps] Malformed options: missing or invalid length byte.\n");
        }
            
        switch (opt_kind)
        {
            case(TCPOPT_MAXSEG): // Maximum Segment Size
                break; 

            case(TCPOPT_WINDOW): // Window scale
                break;

            case(TCPOPT_SACK_PERMITTED): // Selective ACK (SACK) permitted
                break;

            case(TCPOPT_SACK): // SACK
                break;

            case(TCPOPT_TIMESTAMP):
                if (opt_size == TCPOLEN_TIMESTAMP)
                {
                uint32_t raw_val, raw_ecr;
                memcpy(&raw_val, opt_ptr + 2, 4);
                memcpy(&raw_ecr, opt_ptr + 6, 4);

                *ts_val = ntohl(raw_val);
                *ts_ecr = ntohl(raw_ecr);

                return true;
                }
                break;

            default:
                break;
        }

        opt_ptr += opt_size;
        opt_len -= opt_size;
    }
    return false;
}
