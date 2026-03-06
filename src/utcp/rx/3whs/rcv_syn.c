#include <utcp/rx/3whs/rcv_syn.h>

#include <stdbool.h>
#include <stdio.h>

#include <arpa/inet.h>

#include <tcp/hndshk_fsm.h>
#include <utils/err.h>
#include <utils/logger.h>
#include <utils/printable.h>
#include <utcp/api/conn.h>
#include <utcp/rx/find_timestamps.h>
#include <utcp/api/globals.h>
#include <utcp/api/tx_dgram.h>
#include <utcp/api/utcp_timers.h>


void rcv_syn(
    tcb_t *listen_tcb,
    int udp_fd,
    struct tcphdr *hdr,
    ssize_t data_len,
    struct sockaddr_in from
)
{
        if (data_len > 0)
            err_data("[rcv_syn] SYN packet contains non-header data in its payload");

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
        LOG_INFO("[rcv_syn] Received valid SYN. Spawning new TCB...\n");
        tcb_t *new_tcb = alloc_new_tcb();

        // would be better to write this check in its own function so that it doesn't remove the existing
        // TCB from the SYN queue if it passes
        pthread_mutex_lock(&listen_tcb->syn_q.lock);
        if (remove_from_syn_queue(&listen_tcb->syn_q, dest_ip, dest_utcp_port) != NULL)
        {
            LOG_INFO("Incoming request is already in SYN queue; ignoring this packet\n");
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

        LOG_INFO("new TCB:\n");
        print_tcb(new_tcb);

        // add the new TCB to the SYN queue
        pthread_mutex_lock(&listen_tcb->syn_q.lock);
        enqueue_tcb(new_tcb, &listen_tcb->syn_q);
        pthread_mutex_unlock(&listen_tcb->syn_q.lock);
        
        LOG_INFO("[rcv_syn] Sending SYN-ACK\n");
        send_dgram(new_tcb, udp_fd, NULL, 0, TH_SYN | TH_ACK);
}
