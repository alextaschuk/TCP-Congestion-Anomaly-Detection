#include <utcp/rx/3whs/rcv_syn_ack.h>

#include <stdio.h>

#include <tcp/hndshk_fsm.h>

#include <utcp/api/utcp_timers.h>
#include <utcp/api/tx_dgram.h>
#include <utcp/rx/find_timestamps.h>

#include <utils/err.h>


void rcv_syn_ack(
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

        pause_timer(tcb, TCPT_REXMT);
        printf("[rcv_syn_ack] Client received ACK for its SYN. REXMT timer paused.\n");

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
