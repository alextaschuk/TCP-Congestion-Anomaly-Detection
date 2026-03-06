#include <utcp/rx/3whs/rcv_3whs_ack.h>

#include <stdio.h>

#include <tcp/hndshk_fsm.h>
#include <utils/err.h>
#include <utils/logger.h>
#include <utcp/api/conn.h>
#include <utcp/rx/find_timestamps.h>
#include <utcp/api/globals.h>


void rcv_3whs_ack(tcb_t *target_tcb, struct tcphdr *hdr, struct sockaddr_in from)
{
    if (hdr->th_ack != target_tcb->snd_nxt)
            err_data("[rcv_3whs_ack] ACK header's th_ack is not equal to TCB's snd_nxt\n");

    tcb_t *listen_tcb = find_listen_tcb();
    uint32_t remote_ip = ntohl(from.sin_addr.s_addr);
    uint16_t remote_utcp_port = hdr->th_sport;

    pause_timer(target_tcb, TCPT_REXMT);

    LOG_INFO("[rcv_3whs_ack] Server Received ACK. 3WHS done.\n");

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

    LOG_INFO("[rcv_3whs_ack] Connection established and queued for accept().\n");
}
