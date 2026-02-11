/**
 * @brief This API contains functions, variables, etc. that are used 
 * by the server and the client.
 */
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

void deserialize_tcp_hdr
( // TODO: see if this can be made more readable
    uint8_t *buf,
    size_t buflen,
    struct tcphdr **out_hdr,
    uint8_t **out_data,
    ssize_t *out_data_len
)
{
    if (buflen < sizeof(struct tcphdr))
        err_sys("[deserialize_tcp_hdr]cannot parse datagram");

    // copy into local struct so that original header isn't messed with
    struct tcphdr *hdr = (struct tcphdr *)buf;

    // Convert header fields from network byte order to host byte order
    hdr->th_sport = ntohs(hdr->th_sport); // src port
    hdr->th_dport = ntohs(hdr->th_dport); // dest port
    hdr->th_seq = ntohl(hdr->th_seq); // seq #
    hdr->th_ack = ntohl(hdr->th_ack); // ack #
    hdr->th_win = ntohs(hdr->th_win); // rcv window size
    hdr->th_sum = ntohs(hdr->th_sum); // checksum
    hdr->th_urp = ntohs(hdr->th_urp); // urgent pointer

    // TCP header len in bytes
    size_t hdr_len = hdr->th_off * 4;
    if (hdr_len < sizeof(struct tcphdr) || hdr_len > buflen)
        err_sys("[deserialize_tcp_hdr] invalid TCP header length");

    *out_hdr = hdr;
    *out_data = buf + hdr_len;
    *out_data_len = buflen - hdr_len;
}

struct tcb_t *get_tcb(int utcp_fd)
{
    if (utcp_fd < 0 || utcp_fd >= MAX_UTCP_SOCKETS)
        err_sys("[get_tcb]Invalid lookup table position");
    api_t *global = api_instance();
    return global->tcp_lookup[utcp_fd];
}


void update_fsm(int utcp_fd, enum conn_state state)
{ 
    tcb_t *tcb = get_tcb(utcp_fd);
    if (tcb == NULL)
        err_sys("[update_fsm]Invalid UTCP socket");
    
    tcb->fsm_state = state;
}