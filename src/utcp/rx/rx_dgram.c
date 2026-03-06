#include <utcp/rx/rx_dgram.h>

#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>

#include <utcp/api/api.h>
#include <utcp/api/globals.h>

#include <utcp/rx/demux_tcb.h>
#include <utcp/rx/3whs/rcv_3whs_ack.h>
#include <utcp/rx/3whs/rcv_syn_ack.h>
#include <utcp/rx/3whs/rcv_syn.h>
#include <utcp/rx/handle_data.h>

#include <tcp/hndshk_fsm.h>

#include <utils/err.h>
#include <utils/printable.h>
#include <utils/logger.h>


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
    
    LOG_INFO("Received a packet from %s:%d\n", ip_str, ntohs(from.sin_port));
    print_segment(buf, rcvsize, 1);

    // deserialize & demux the packet
    deserialize_utcp_packet(buf, rcvsize, &hdr, &data, &data_len);

    uint16_t local_utcp_port = hdr->th_dport;
    uint32_t remote_ip = ntohl(from.sin_addr.s_addr);
    uint16_t remote_udp_port = ntohs(from.sin_port);

    tcb_t *target_tcb = demux_tcb(global, local_utcp_port, remote_ip, remote_udp_port);

    if (target_tcb == NULL)
    {
        LOG_WARN("[rcv_dgram] No matching TCB found for port %u. Dropping the packet.\n", local_utcp_port);
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
                LOG_ERROR("[rcv_syn] Expected SYN flag, but none was found");
            break;

        case SYN_SENT:
            if ((hdr->th_flags & TH_SYN) && (hdr->th_flags & TH_ACK))
                rcv_syn_ack(target_tcb, udp_fd, hdr, data_len);
            else
                LOG_ERROR("[rcv_syn_ack] Missing SYN and/or ACK flag(s) for SYN-ACK");
            break;

        case SYN_RECEIVED:
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

    free(buf);
    return rcvsize;
}


