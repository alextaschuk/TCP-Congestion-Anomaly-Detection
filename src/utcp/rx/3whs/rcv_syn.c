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

        uint32_t dest_ip = ntohl(from.sin_addr.s_addr);
        uint16_t dest_utcp_port = hdr->th_sport;
        uint16_t dest_udp_port = ntohs(from.sin_port);

        uint16_t src_utcp_port = listen_tcb->fourtuple.source_port;
        
        /* create a new TCB for the incoming connection request */
        LOG_INFO("[rcv_syn] Received a valid SYN. Spawning new TCB...", dest_ip, hdr->th_sport);
        tcb_t *new_tcb = alloc_new_tcb();

        // would be better to write this check in its own function so that it doesn't remove the existing
        // TCB from the SYN queue if it passes
        //LOG_DEBUG("[rcv_syn] Locking to SYN queue to check if the incoming request is already in the queue...");
        pthread_mutex_lock(&listen_tcb->syn_q.lock);

        if (remove_from_syn_queue(&listen_tcb->syn_q, dest_ip, dest_utcp_port) != NULL)
        {
            LOG_WARN("Incoming request is already in SYN queue; ignoring this packet and unlocking the SYN queue...");
            pthread_mutex_unlock(&listen_tcb->syn_q.lock);
            return;
        }

        //LOG_DEBUG("[rcv_syn] Connection request not found in the SYN queue. Unlocking the SYN queue...");
        //pthread_mutex_unlock(&listen_tcb->syn_q.lock);

        //LOG_DEBUG("[rcv_syn] Locking the new connection's TCB (fd=%i) to configure it...", new_tcb->fd);
        //pthread_mutex_lock(&new_tcb->lock);

        /* configure the new connection's TCB */
        new_tcb->fourtuple.dest_port = dest_utcp_port;
        new_tcb->fourtuple.dest_ip = dest_ip;
        new_tcb->dest_udp_port = dest_udp_port;
        new_tcb->fourtuple.source_port = src_utcp_port;

        new_tcb->src_udp_fd = udp_fd;

        new_tcb->irs = hdr->th_seq;
        new_tcb->rcv_nxt = new_tcb->irs + 1;
        new_tcb->rcv_wnd = BUF_SIZE;

        new_tcb->iss = 500; // TODO: replace with randomly generated SEQ num
        new_tcb->snd_una = new_tcb->iss;
        new_tcb->snd_nxt = new_tcb->iss;
        new_tcb->snd_max = new_tcb->iss;
        new_tcb->snd_wnd = hdr->th_win;
        new_tcb->fsm_state = SYN_RECEIVED;

        /* Check for TCP Options section and if timestamps are present */
        uint32_t ts_val = 0; // Timestamp value in rcv'd packet
        uint32_t ts_ecr = 0; // Echoed timestamp value in rcv'd packet

        bool has_ts_opt = find_timestamps(hdr, &ts_val, &ts_ecr);

        if (has_ts_opt)
        {
            uint32_t old_ts_val = new_tcb->ts_rcv_val;
            new_tcb->ts_rcv_val = ts_val;
            LOG_INFO("[rcv_syn] Updated ts_rcv_val from %u to %u", old_ts_val, new_tcb->ts_rcv_val);
        }

        //LOG_DEBUG("[rcv_sny] Successfully configured the new TCB with fd=%i. Unlocking the TCB...", new_tcb->fd);
        log_tcb(new_tcb, "");
        

        /* add the new TCB to the SYN queue */
        enqueue_tcb(new_tcb, &listen_tcb->syn_q);
        //LOG_DEBUG("[rcv_syn] Added the incoming request with fd=%i to the SYN queue. Unlocking the SYN queue...", new_tcb->fd);
        pthread_mutex_unlock(&listen_tcb->syn_q.lock);
        
        pthread_mutex_lock(&new_tcb->lock);
        //LOG_INFO("[rcv_syn] Locked the new TCB and sending a SYN-ACK to incoming request with fd=%i.", new_tcb->fd);

        send_dgram(new_tcb);

        //LOG_INFO("[rcv_syn] TCB with fd=%d sent SYN-ACK. Unlocking this TCB...", new_tcb->fd);
        pthread_mutex_unlock(&new_tcb->lock);
        
        //LOG_INFO("[rcv_syn] SYN-ACK sent for new connection with fd=%i. Unlocking the new TCB...", new_tcb->fd);
        //pthread_mutex_unlock(&new_tcb->lock);
}
