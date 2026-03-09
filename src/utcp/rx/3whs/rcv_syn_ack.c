#include <utcp/rx/3whs/rcv_syn_ack.h>

#include <stdio.h>

#include <tcp/hndshk_fsm.h>
#include <utils/err.h>
#include <utils/logger.h>
#include <utcp/rx/find_timestamps.h>
#include <utcp/api/globals.h>
#include <utcp/api/tx_dgram.h>
#include <utcp/api/utcp_timers.h>


void rcv_syn_ack(
    tcb_t *tcb,
    int udp_fd,
    struct tcphdr *hdr,
    ssize_t data_len
)
{
    if (data_len > 0) // we won't be using TCP Fast Open
        err_data("[rcv_syn_ack] ACK contains non-header data");

    //LOG_DEBUG("[rcv_syn_ack] Locking the TCB with fd=%i to update it using the SYN-ACK segment...", tcb->fd);
    //pthread_mutex_lock(&tcb->lock);
    
    if (hdr->th_ack != tcb->snd_nxt)
    {
        LOG_ERROR("[rcv_syn_ack] Received ACK is not equal to snd_nxt");
        return;
    }

    tcb->irs = hdr->th_seq;
    tcb->snd_una = hdr->th_ack;
    tcb->rcv_nxt = hdr->th_seq + 1;
    tcb->rcv_wnd = hdr->th_win;
    tcb->fsm_state = ESTABLISHED;

    pause_timer(tcb, TCPT_REXMT);
    LOG_INFO("[rcv_syn_ack] Client received SYN-ACK. REXMT timer paused.\n");

    /* check for TCP Options section and if timestamps are present */
    uint32_t ts_val = 0; // Timestamp value
    uint32_t ts_ecr = 0; // Echoed timestamp value

    bool has_ts_opt = find_timestamps(hdr, &ts_val, &ts_ecr);

    if (has_ts_opt)
    {
        tcb->ts_rcv_val = ts_val;
    }

    // stop the retransmission timer
    tcb->t_timer[TCPT_REXMT] = 0;
    tcb->rxtshift = 0;

    tcb->t_flags |= F_ACKNOW;
    
    LOG_DEBUG("[rcv_syn_ack] Sending ACK for SYN-ACK...");
    send_dgram(tcb);
    
    LOG_DEBUG("[rcv_syn_ack] Finished updating the TCB with fd=%i using SYN-ACK segment. Waking up thread blocking in utcp_connect...", tcb->fd);
    pthread_cond_broadcast(&tcb->conn_cond); // wake up client thread that is blocking in utcp_connect()
    //pthread_mutex_unlock(&tcb->lock);
}
