#include <utcp/rx/rx_dgram.h>

#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>

#include <tcp/hndshk_fsm.h>
#include <tcp/tcb.h>
#include <utils/err.h>
#include <utils/logger.h>
#include <utils/printable.h>
#include <utcp/api/api.h>
#include <utcp/api/conn.h>
#include <utcp/api/globals.h>
#include <utcp/rx/demux_tcb.h>
#include <utcp/rx/handle_data.h>
#include <utcp/rx/handle_tcp_options.h>
#include <utcp/api/tx_dgram.h>


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
    {
        free(buf);
        LOG_ERROR("[rcv_dgram] Failed to allocate receive buffer");
    }

    for(;;)
    { // continuously listen for incoming packets
        rcvsize = recvfrom(udp_fd, buf, (size_t)buflen, 0, (struct sockaddr *)&from, &fromlen);

        if (rcvsize < 0 || rcvsize == 0)
        { // rcvsize == 0 would normally indicate a connection shutdown process
            free(buf);
            LOG_ERROR("[rcv_dgram] Failed to receive datagram");
            break;
        }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from.sin_addr, ip_str, sizeof(ip_str));
    
        //LOG_INFO("[rcv_dgram] Received a packet from %s:%d", ip_str, ntohs(from.sin_port));
        log_segment(buf, rcvsize, 1, "");

        deserialize_utcp_packet(buf, rcvsize, &hdr, &data, &data_len);

        uint16_t local_utcp_port = hdr->th_dport;
        uint32_t remote_ip = ntohl(from.sin_addr.s_addr);
        uint16_t remote_udp_port = ntohs(from.sin_port);

        tcb_t *target_tcb = demux_tcb(global, local_utcp_port, remote_ip, remote_udp_port);
        if (target_tcb == NULL)
        {
            free(buf);
            LOG_WARN("[rcv_dgram] No matching TCB found for port %u. Dropping the packet.", local_utcp_port);
            return rcvsize;
        }

        //LOG_DEBUG("[rcv_dgram] Locking the TCB while the received segment is handled.");
        pthread_mutex_lock(&target_tcb->lock);

        switch (target_tcb->fsm_state)
        {
            case LISTEN: // Received a SYN; sends SYN-ACK
                if (hdr->th_flags & TH_SYN)
                {
                    if (data_len > 0)
                        LOG_ERROR("[rcv_dgram] SYN packet contains non-header data in its payload");

                    uint16_t dest_utcp_port = hdr->th_sport;
                    uint32_t dest_ip = ntohl(from.sin_addr.s_addr);
                    uint16_t dest_udp_port = ntohs(from.sin_port);
                    uint16_t src_utcp_port = target_tcb->fourtuple.source_port;
        
                    LOG_INFO("[rcv_dgram] Received a valid SYN. Creating new TCB and placing it in the SYN queue...", dest_ip, hdr->th_sport);
                    tcb_t *new_tcb = alloc_new_tcb();

                    // would be better to write this check in its own function so that it doesn't remove the existing
                    // TCB from the SYN queue if it passes
                    pthread_mutex_lock(&target_tcb->syn_q.lock);

                    if (remove_from_syn_queue(&target_tcb->syn_q, dest_ip, dest_utcp_port) != NULL)
                    {
                        LOG_WARN("Incoming request is already in SYN queue; ignoring this packet and unlocking the SYN queue...");
                        pthread_mutex_unlock(&target_tcb->syn_q.lock);
                        return -1;
                    }

                    /* configure the new connection's TCB */
                    new_tcb->fourtuple.dest_port = dest_utcp_port;
                    new_tcb->fourtuple.dest_ip = dest_ip;
                    new_tcb->dest_udp_port = dest_udp_port;
                    new_tcb->fourtuple.source_port = src_utcp_port;

                    new_tcb->src_udp_fd = udp_fd;

                    new_tcb->irs = hdr->th_seq;
                    new_tcb->rcv_nxt = new_tcb->irs + 1;
                    new_tcb->rcv_wnd = BUF_SIZE;

                    new_tcb->snd_wnd = hdr->th_win;
                    new_tcb->fsm_state = SYN_RECEIVED;

                    process_tcp_options(new_tcb, hdr, true);
                    enqueue_tcb(new_tcb, &target_tcb->syn_q);

                    pthread_mutex_unlock(&target_tcb->syn_q.lock);
                    log_tcb(new_tcb, "New TCB for received SYN:");

                    pthread_mutex_lock(&new_tcb->lock);
                    send_dgram(new_tcb);
                    pthread_mutex_unlock(&new_tcb->lock);
            }
            else
                LOG_ERROR("[rcv_dgram] Expected SYN flag, but none was found");
            break;

            case SYN_SENT: // Recevied a SYN-ACK; sends ACK
                if ((hdr->th_flags & TH_SYN) && (hdr->th_flags & TH_ACK))
                {
                    if (data_len > 0) // we won't be using TCP Fast Open
                    {
                        LOG_ERROR("[rcv_dgram] ACK contains non-header data");
                        return -1;
                    }

                    if (hdr->th_ack != target_tcb->snd_nxt)
                    {
                        LOG_ERROR("[rcv_dgram] Received ACK=%u is not equal to snd_nxt=%u", hdr->th_ack, target_tcb->snd_nxt);
                        return -1;
                    }

                    target_tcb->irs = hdr->th_seq;
                    target_tcb->snd_una = hdr->th_ack;
                    target_tcb->rcv_nxt = hdr->th_seq + 1;
                    target_tcb->rcv_wnd = hdr->th_win;
                    target_tcb->fsm_state = ESTABLISHED;

                    
                    process_tcp_options(target_tcb, hdr, true);
                    target_tcb->snd_wnd = GET_SCALED_WIN(target_tcb, hdr);
                    
                    //LOG_INFO("[rcv_dgram] Client received SYN-ACK. REXMT timer paused.");
                    pause_timer(target_tcb, TCPT_REXMT);
                    target_tcb->rxtshift = 0;

                    target_tcb->t_flags |= F_ACKNOW;
                    send_dgram(target_tcb);

                    //LOG_DEBUG("[rcv_dgram] Finished updating the TCB with fd=%i using SYN-ACK segment. Waking up thread blocking in utcp_connect...", target_tcb->fd);
                    pthread_cond_broadcast(&target_tcb->conn_cond); // wake up client thread that is blocking in utcp_connect()
                }
                else
                    LOG_ERROR("[rcv_dgram] Missing SYN and/or ACK flag(s) for SYN-ACK");
                break;

            case SYN_RECEIVED: // Received ACK for SYN-ACK; start sending data
                if (hdr->th_flags & TH_ACK)
                {
                    if (hdr->th_ack != target_tcb->snd_nxt)
                    {
                        LOG_ERROR("[rcv_dgram] ACK header's th_ack=%u is not equal to TCB's snd_nxt=%u", hdr->th_ack, target_tcb->snd_nxt);
                        return -1;
                    }

                    tcb_t *listen_tcb = find_listen_tcb();
                    uint32_t remote_ip = ntohl(from.sin_addr.s_addr);
                    uint16_t remote_utcp_port = hdr->th_sport;

                    pause_timer(target_tcb, TCPT_REXMT);
                    target_tcb->rxtshift = 0;

                    process_tcp_options(target_tcb, hdr, true);
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

                    LOG_INFO("[rcv_dgram] Connection established and queued for utcp_accept()");
                }
                else
                    LOG_ERROR("[rcv_dgram] Expected ACK flag, but none was found");
                break;

            case ESTABLISHED: // 3WHS is complete; handle segment accordingly
                handle_data(target_tcb, hdr, data, data_len);
                break;

            default:
                LOG_ERROR("[rcv_dgram] TCB's FSM is not in a valid state to receive data");
            }
            //LOG_DEBUG("[rcv_dgram] Unlocking the TCB.");
            pthread_mutex_unlock(&target_tcb->lock);
    }
    free(buf);
    return rcvsize;
}
