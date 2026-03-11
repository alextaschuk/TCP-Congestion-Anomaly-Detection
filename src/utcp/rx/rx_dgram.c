#include <utcp/rx/rx_dgram.h>

#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>

#include <tcp/hndshk_fsm.h>
#include <utils/err.h>
#include <utils/logger.h>
#include <utils/printable.h>
#include <utcp/api/api.h>
#include <utcp/rx/demux_tcb.h>
#include <utcp/api/globals.h>
#include <utcp/rx/handle_data.h>
#include <utcp/rx/3whs/rcv_3whs_ack.h>
#include <utcp/rx/3whs/rcv_syn_ack.h>
#include <utcp/rx/3whs/rcv_syn.h>


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

    for(;;)
    { // continuously listen for incoming packets
    rcvsize = recvfrom(udp_fd, buf, (size_t)buflen, 0, (struct sockaddr *)&from, &fromlen);

    if (rcvsize < 0)
    {
        free(buf);
        err_sys("[rcv_dgram] Failed to receive datagram");
    }

    if (rcvsize == 0)
    {
        free(buf);
        printf("TODO handle connection shutdown process");
        return rcvsize;
    }

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &from.sin_addr, ip_str, sizeof(ip_str));
    
    LOG_INFO("[rcv_dgram] Received a packet from %s:%d", ip_str, ntohs(from.sin_port));
    log_segment(buf, rcvsize, 1, "[rcv_dgram] Received segment:");

    // deserialize & demux the packet
    deserialize_utcp_packet(buf, rcvsize, &hdr, &data, &data_len);

    uint16_t local_utcp_port = hdr->th_dport;
    uint32_t remote_ip = ntohl(from.sin_addr.s_addr);
    uint16_t remote_udp_port = ntohs(from.sin_port);

    tcb_t *target_tcb = demux_tcb(global, local_utcp_port, remote_ip, remote_udp_port);
    if (target_tcb == NULL)
    {
        LOG_WARN("[rcv_dgram] No matching TCB found for port %u. Dropping the packet.", local_utcp_port);
        free(buf);
        return rcvsize;
    }

    //LOG_DEBUG("[rcv_dgram] Locking the TCB while the received segment is handled.");
    pthread_mutex_lock(&target_tcb->lock);

    switch (target_tcb->fsm_state)
    {
        case LISTEN: // waiting for incoming SYN; sends SYN-ACK
            if (hdr->th_flags & TH_SYN)
                rcv_syn(target_tcb, udp_fd, hdr, data_len, from);
            else
                LOG_ERROR("[rcv_syn] Expected SYN flag, but none was found");
            break;

        case SYN_SENT: // waiting for incoming SYN-ACK; sends ACK
            if ((hdr->th_flags & TH_SYN) && (hdr->th_flags & TH_ACK))
                rcv_syn_ack(target_tcb, udp_fd, hdr, data_len);
            else
                LOG_ERROR("[rcv_syn_ack] Missing SYN and/or ACK flag(s) for SYN-ACK");
            break;

        case SYN_RECEIVED: // waiting for ACK; start sending data
            if (hdr->th_flags & TH_ACK)
                rcv_3whs_ack(target_tcb, hdr, from);
            else
                LOG_ERROR("[rcv_ack] Expected ACK flag, but none was found");
            break;

        case ESTABLISHED: // 3WHS is complete; received packet containing data
            handle_data(target_tcb, udp_fd, hdr, data, data_len);
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


