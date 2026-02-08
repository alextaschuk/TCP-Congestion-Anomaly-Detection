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

void deserialize_tcp_hdr(uint8_t* buf, size_t buflen, struct tcphdr **out_hdr, uint8_t **out_data, ssize_t *out_data_len)
{
    /**
     * @brief deserialize a TCP header back into 
     * host byte order
     */
    if (buflen < sizeof(struct tcphdr))
        err_sys("[deserialize_tcp_hdr]cannot parse datagram");

    *out_hdr = (struct tcphdr *)buf;

    // Convert header fields from network byte order to host byte order
    (*out_hdr)->th_sport = ntohs((*out_hdr)->th_sport); // src port
    (*out_hdr)->th_dport = ntohs((*out_hdr)->th_dport); // dest port
    (*out_hdr)->th_seq = ntohl((*out_hdr)->th_seq); // seq #
    (*out_hdr)->th_ack = ntohl((*out_hdr)->th_ack); // ack #
    (*out_hdr)->th_win = ntohs((*out_hdr)->th_win); // rcv window size
    (*out_hdr)->th_sum = ntohs((*out_hdr)->th_sum); // checksum
    (*out_hdr)->th_urp = ntohs((*out_hdr)->th_urp); // urgent pointer

    //print_tcphdr(*out_hdr);
}

struct tcb* get_tcb(int utcp_fd)
{
    /**
     * @brief Retrieves the TCB for a UTCP socket at tcb_lookup[pos]
     */
    if (utcp_fd < 0 || utcp_fd >= MAX_UTCP_SOCKETS)
        err_sys("[get_tcb]Invalid lookup table position");
    api_t *global = api_instance();
    return global->tcp_lookup[utcp_fd];
}


void update_fsm(int utcp_fd, enum conn_state state)
{ 
    /**
     * @brief update the FSM state of a given UTCP socket
     */
    struct tcb *tcb = get_tcb(utcp_fd);
    if (tcb == NULL)
        err_sys("[update_fsm]Invalid UTCP socket");
    
    tcb->fsm_state = state;
}
