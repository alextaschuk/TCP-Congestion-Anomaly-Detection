#include <utils/printable.h>

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h> 

#include <tcp/tcp_segment.h>
#include <utils/err.h>
#include <utils/logger.h>
#include <utcp/api/api.h>
#include <utcp/api/globals.h>

/**
 * @brief helper function to determine if the struct to be printed is `NULL` or not.
 * 
 * @param *ptr A pointer of any type.
 * @param *msg A string to print if `*ptr` is indeed `NULL`.
 * 
 * @returns `true` if `*ptr` is `NULL`, `false` otherwise.
 */
static bool is_null(const void *ptr, const char *msg)
{
    if (ptr == NULL)
    {
        LOG_INFO("(is_null) %s", msg);
        return true;
    }
    return false;
        
}


void log_segment(const uint8_t *buf, const size_t buflen, const bool flow, char *msg)
{
    if (is_null(buf, "TCP Header: (null)"))
        return;

    if (flow != 0 && flow != 1)
        err_sys("[log_segment] Invalid direction");

    uint8_t *local_buf = malloc(buflen);
    if (!local_buf)
        err_sys("[log_segment] malloc for local_buf failed.\n");
    memcpy(local_buf, buf, buflen);

    struct tcphdr *hdr;
    uint8_t* payload;
    ssize_t payload_len;
    deserialize_utcp_packet(local_buf, buflen, &hdr, &payload, &payload_len);

    const char *direction = flow ? "\n<<< [INCOMING PACKET]" : "\n[OUTGOING PACKET] >>>";

    size_t hdrlen = hdr->th_off * 4;
    size_t opt_len = hdrlen - sizeof(struct tcphdr);
    
    /* Create a buffer large enough to hold the formatted string */
    char log_buf[2048];
    size_t offset = 0;

    /* Write the base header into the buffer */
    offset += snprintf(log_buf + offset, sizeof(log_buf) - offset,
        "%s\n" // msg[]
        "%s\n" // direction
        "--------------------Header-------------------\n"
        "\tSrc UTCP Port    : %u\n"
        "\tDest UTCP Port   : %u\n"
        "\tSequence Number  : %u\n"
        "\tAck Number       : %u\n"
        "\tFlags            : [ %s%s%s%s%s%s]\n"
        "\tWindow           : %u\n"
        "\tSize of segment  : %zu\n"
        "\tSize of payload  : %zu\n",
        msg,
        direction,
        hdr->th_sport,
        hdr->th_dport,
        hdr->th_seq,
        hdr->th_ack,
        (hdr->th_flags & TH_SYN)  ? "SYN " : "",
        (hdr->th_flags & TH_ACK)  ? "ACK " : "",
        (hdr->th_flags & TH_FIN)  ? "FIN " : "",
        (hdr->th_flags & TH_RST)  ? "RST " : "",
        (hdr->th_flags & TH_PUSH) ? "PSH " : "",
        (hdr->th_flags & TH_URG)  ? "URG " : "",
        hdr->th_win,
        buflen,
        payload_len
    );

    /* Append options to the buffer */
    if (opt_len > 0) 
    {
        offset += snprintf(log_buf + offset, sizeof(log_buf) - offset, "\tOptions\t\t\t :\n");
        
        uint8_t *opt_ptr = (uint8_t *)hdr + sizeof(struct tcphdr);
        size_t remaining = opt_len;

        while (remaining > 0) 
        {
            uint8_t opt_kind = opt_ptr[0];

            if (opt_kind == 0) // TCPOPT_EOL
            { 
                offset += snprintf(log_buf + offset, sizeof(log_buf) - offset, "\t\t- EOL (End of Options)\n");
                break;
            } 
            else if (opt_kind == 1) // TCPOPT_NOP
            { 
                offset += snprintf(log_buf + offset, sizeof(log_buf) - offset, "\t\t- NOP (Padding)\n");
                opt_ptr++;
                remaining--;
                continue;
            }
         
            uint8_t opt_size = opt_ptr[1];
            if (opt_size < 2 || opt_size > remaining)
            {
                offset += snprintf(log_buf + offset, sizeof(log_buf) - offset, "\t\tMalformed options: missing or invalid length byte.\n");
                break;
            }

            switch (opt_kind)
            {
                case(TCPOPT_MAXSEG):
                    if (opt_size == TCPOLEN_MAXSEG)
                    {
                        uint16_t mss;
                        memcpy(&mss, opt_ptr + 2, 2);
                        offset += snprintf(log_buf + offset, sizeof(log_buf) - offset, "\t\t- MSS: %u\n", ntohs(mss));
                    }
                    break;

                case(TCPOPT_WINDOW):
                    if (opt_size == TCPOLEN_WINDOW)
                    {
                        uint8_t shift_count = opt_ptr[2];
                        // RFC 7323 caps the functional shift at 14, but we print the raw wire value
                        uint8_t calc_shift = (shift_count > 14) ? 14 : shift_count;
                        uint32_t multiplier = 1 << calc_shift;
                        
                        offset += snprintf(log_buf + offset, sizeof(log_buf) - offset, "\t\t- Window Scale : Shift = %u (Multiplier = %u)\n", shift_count, multiplier);
                    }
                    break;

                case(TCPOPT_TIMESTAMP):
                    if (opt_size == TCPOLEN_TIMESTAMP)
                    { 
                        uint32_t ts_val, ts_ecr;
                        memcpy(&ts_val, opt_ptr + 2, 4);
                        memcpy(&ts_ecr, opt_ptr + 6, 4);
            
                        offset += snprintf(log_buf + offset, sizeof(log_buf) - offset, "\t\t- Timestamps : TSval = %u ms, TSecr = %u ms\n", ntohl(ts_val),  ntohl(ts_ecr));
                    }
                    break;

                // (Ignored known options removed for brevity, they fall to default if needed or just break)
                case(TCPOPT_SACK_PERMITTED):
                case(TCPOPT_SACK):
                    break;

                default:
                    offset += snprintf(log_buf + offset, sizeof(log_buf) - offset, "\t\t- Unknown Option\t: Type = %u, Length = %u\n", opt_kind, opt_size);
            }
            opt_ptr += opt_size;
            remaining -= opt_size;
        }
    }

    /* print payload characters */
    //if (payload_len > 0 && payload != NULL)
    //{
    //    offset += snprintf(log_buf + offset, sizeof(log_buf) - offset, "\tPayload Data\t : ");
    //    size_t display_len = payload_len;
    //    
    //    for (size_t i = 0; i < display_len; i++) 
    //    {
    //        if (isprint(payload[i])) // isprint() ensures we only log safe ASCII characters to the string
    //            offset += snprintf(log_buf + offset, sizeof(log_buf) - offset, "%c", payload[i]);
    //        else
    //            offset += snprintf(log_buf + offset, sizeof(log_buf) - offset, ".");
    //    }
    //
    //    offset += snprintf(log_buf + offset, sizeof(log_buf) - offset, "\n");
    //}

    offset += snprintf(log_buf + offset, sizeof(log_buf) - offset, "--------------------------------------------\n");

    LOG_INFO("%s", log_buf);
    free(local_buf);
}


void log_tcb(const tcb_t *tcb, char *msg)
{
    if (is_null(tcb, "TCB: (null)"))
        return;

    char src_ip[INET_ADDRSTRLEN];
    char dest_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &tcb->fourtuple.source_ip, src_ip, sizeof(src_ip));
    inet_ntop(AF_INET, &tcb->fourtuple.dest_ip, dest_ip, sizeof(dest_ip));

    LOG_INFO(
        "%s\n"
        "\n~~~~~~~~~~~~~~~~~~~TCB~~~~~~~~~~~~~~~~~~\n"
        "\tFour Tuple:\n"
        "\t\tsrc (UTCP) port   : %u\n"
        "\t\tsrc IP            : %s\n"
        "\t\tdest (UTCP) port  : %u\n"
        "\t\tdest IP           : %s\n"
        "\tsrc_udp_port        : %u\n"
        "\tdest_udp_port       : %u\n"
        "\tfsm_state           : %s\n"
        "\tUTCP file descriptor: %u\n"
        "\tUDP file descriptor : %u\n"
        "\n"
        "\tiss                 : %u\n"
        "\tsnd_una             : %u\n"
        "\tsnd_nxt             : %u\n"
        "\tsnd_max             : %u\n"
        "\tsnd_wnd             : %u\n"
        "\tirs                 : %u\n"
        "\trcv_nxt             : %u\n"
        "\trwnd                : %u\n"
        "\n"
        "\tws_enabled          : %d ms\n"
        "\tsnd_ws_scale        : %d ms\n"
        "\trcv_ws_scale        : %d ms\n"
        "\n"
        "\tts_rcv_val          : %u ms\n"
        "\n"
        "\trxtcur              : %u\n"
        "\tsrtt                : %u\n"
        "\trttvar              : %u\n"
        "\n"
        "\tcwnd                : %u\n"
        "\tssthresh            : %u\n"
        "\tdupacks             : %u\n"
        "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n",
        msg,

        tcb->fourtuple.source_port,
        src_ip,
        tcb->fourtuple.dest_port,
        dest_ip,
        
        tcb->src_udp_port,
        tcb->dest_udp_port,
        fsm_state_to_str(tcb->fsm_state),
        tcb->fd,
        tcb->src_udp_fd,
        
        tcb->iss,
        tcb->snd_una,
        tcb->snd_nxt,
        tcb->snd_max,
        tcb->snd_wnd,
        tcb->irs,
        tcb->rcv_nxt,
        tcb->rwnd,
        
        tcb->ws_enabled,
        tcb->snd_ws_scale,
        tcb->rcv_ws_scale,

        tcb->ts_rcv_val,

        tcb->rxtcur,
        tcb->srtt,
        tcb->rttvar,

        tcb->cwnd,
        tcb->ssthresh,
        tcb->dupacks
    );
}


void print_safe_chars(uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        char c = (char)buf[i];
        printf("%c", isprint(c) ? c : '.'); // non-printable bytes become '.'
    }

    printf("\n");
}

const char* fsm_state_to_str(enum conn_state state)
{
    switch (state)
    {
        case LISTEN:       return "LISTEN";
        case SYN_SENT:     return "SYN_SENT";
        case SYN_RECEIVED: return "SYN_RECEIVED";
        case ESTABLISHED:  return "ESTABLISHED";
        case FIN_WAIT_1:   return "FIN_WAIT_1";
        case FIN_WAIT_2:   return "FIN_WAIT_2";
        case CLOSE_WAIT:   return "CLOSE_WAIT";
        case CLOSING:      return "CLOSING";
        case LAST_ACK:     return "LAST_ACK";
        case TIME_WAIT:    return "TIME_WAIT";
        case CLOSED:       return "CLOSED";
        default:           return "UNKNOWN_STATE";
    }
}

void print_lookup(void)
{
    api_t *global = api_instance();

    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        log_tcb(global->tcb_lookup[i], "");
    }
}