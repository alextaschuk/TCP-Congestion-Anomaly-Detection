#include <utcp/api/api.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/tcp.h>

#include <utcp/api/globals.h>

#include <tcp/tcp_segment.h>
#include <tcp/hndshk_fsm.h>

#include <utils/err.h>
#include <utils/printable.h>
#include <utils/logger.h>

void deserialize_utcp_packet
(
    uint8_t *buf,
    size_t buflen,
    struct tcphdr **out_hdr,
    uint8_t **out_data,
    ssize_t *out_data_len
)
{
    if (buflen < sizeof(struct tcphdr))
        err_sys("[deserialize_utcp_packet] cannot parse datagram");
    
    *out_hdr = (struct tcphdr *)buf;

    /* Convert header fields from NETWORK byte order to HOST byte order */

    (*out_hdr)->th_sport = ntohs((*out_hdr)->th_sport); // src port
    (*out_hdr)->th_dport = ntohs((*out_hdr)->th_dport); // dest port
    (*out_hdr)->th_seq = ntohl((*out_hdr)->th_seq); // seq #
    (*out_hdr)->th_ack = ntohl((*out_hdr)->th_ack); // ack #
    (*out_hdr)->th_win = ntohs((*out_hdr)->th_win); // rcv window size
    (*out_hdr)->th_sum = ntohs((*out_hdr)->th_sum); // checksum
    (*out_hdr)->th_urp = ntohs((*out_hdr)->th_urp); // urgent pointer

    size_t hdrlen = (*out_hdr)->th_off * 4; // how many 32-bit words the header + options consume

    if (buflen < hdrlen)
        err_sys("[deserialize_utcp_packet] invalid TCP header length: truncated options");

    *out_data = buf + hdrlen;
    *out_data_len = buflen - hdrlen;
}


struct tcb_t *get_tcb(int utcp_fd)
{
    if (utcp_fd < 0 || utcp_fd >= MAX_CONNECTIONS)
    {
        LOG_WARN("[get_tcb] Invalid lookup table position: %i\n", utcp_fd);
        return NULL;
    }

    api_t *global = api_instance();
    tcb_t *tcb = global->tcb_lookup[utcp_fd];

    if (tcb == NULL)
        return NULL;
    
    return tcb;
}


void update_fsm(int utcp_fd, enum conn_state state)
{ 
    tcb_t *tcb = get_tcb(utcp_fd);
    if (tcb == NULL)
        err_sys("[update_fsm]Invalid UTCP socket");
    
    tcb->fsm_state = state;
}