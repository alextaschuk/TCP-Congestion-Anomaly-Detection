#include <utcp/api/api.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <unistd.h>

#include <tcp/hndshk_fsm.h>
#include <tcp/tcp_segment.h>
#include <utils/err.h>
#include <utils/logger.h>
#include <utils/printable.h>
#include <utcp/api/globals.h>

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
        LOG_ERROR("[get_tcb] fd=%i is an invalid lookup table position:", utcp_fd);
        return NULL;
    }

    api_t *global = api_instance();

    pthread_mutex_lock(&global->lookup_lock);
    tcb_t *tcb = global->tcb_lookup[utcp_fd];
    pthread_mutex_unlock(&global->lookup_lock);

    if (tcb == NULL)
    {
        LOG_WARN("[get_tcb] TCB with fd=%u was not found", utcp_fd);
        return NULL;
    }

    //LOG_INFO("[get_tcb] Found the TCB you're looking for at FD %u", utcp_fd);
    return tcb;
}


void update_fsm(int utcp_fd, enum conn_state state)
{ 
    tcb_t *tcb = get_tcb(utcp_fd);
    if (tcb == NULL)
        LOG_ERROR("[update_fsm] TCB with fd=%i is invalid.", utcp_fd);
    
    char *old_state = fsm_state_to_str(tcb->fsm_state);

    tcb->fsm_state = state;

    LOG_DEBUG("[update_fsm] TCB with fd=%i changed from %s to %s", utcp_fd, old_state, fsm_state_to_str(tcb->fsm_state));
}