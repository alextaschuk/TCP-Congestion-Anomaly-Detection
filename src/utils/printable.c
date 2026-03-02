#include <utils/printable.h>

#include <arpa/inet.h> 
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <utils/err.h>
#include <tcp/tcp_segment.h>
#include <utcp/api/api.h>
#include <utcp/api/globals.h>

static bool is_null(const void *ptr, const char *msg)
{
    if (ptr == NULL)
    {
        printf("%s\n", msg);
        return true;
    }
    return false;
        
}

void print_segment(const uint8_t *buf, const size_t buflen, const bool flow)
{
    if (is_null(buf, "TCP Header: (null)"))
        return;

    if (flow != 0 && flow != 1)
        err_sys("[print_segment] Invalid direction");

    // this function is non-destructive, so we need a local copy of the data
    uint8_t *local_buf = malloc(buflen);
    if (!local_buf)
        err_sys("[print_segment] malloc for local_buf failed.\n");
    memcpy(local_buf, buf, buflen);

    struct tcphdr *hdr;
    uint8_t* payload;
    ssize_t payload_len;
    deserialize_utcp_packet(local_buf, buflen, &hdr, &payload, &payload_len);

    const char *direction = flow ? "\n<<< [INCOMING PACKET]" : "\n[OUTGOING PACKET] >>>";

    size_t hdrlen = hdr->th_off * 4;
    size_t opt_len = hdrlen - sizeof(struct tcphdr); // how many bytes of options there are
    
    /* The TCP header */
    printf("----------------------------------------");
    printf("%s\n"
           "  Src UTCP Port    : %u\n"
           "  Dest UTCP Port   : %u\n"
           "  Sequence Number  : %u\n"
           "  Ack Number       : %u\n"
           "  Flags            : [ %s%s%s%s%s%s]\n"
           "  Window           : %u\n"
           "  Size of segment  : %zu\n"
           "  Size of payload  : %zu\n",
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

    /* Print options, if there are any */
    if (opt_len > 0) 
    {
        printf("  Options          :\n");
        
        uint8_t *opt_ptr = (uint8_t *)hdr + sizeof(struct tcphdr); // the 1st option byte right after the TCP header
        size_t remaining = opt_len;

        while (remaining > 0) 
        {
            uint8_t opt_kind = opt_ptr[0];

            /* single-byte (only one) option, no length field */
            if (opt_kind == 0)
            { // TCPOPT_EOL
                printf("    - EOL (End of Options)\n");
                break;
            } 
            else if (opt_kind == 1)
            { // TCPOPT_NOP
                printf("    - NOP (Padding)\n");
                opt_ptr++;
                remaining--;
                continue;
            }

            /* multi-byte (several) options */            
            uint8_t opt_size = opt_ptr[1];
            if (opt_size < 2 || opt_size > remaining)
                printf("[print_segment] Malformed options: missing or invalid length byte.\n");

            switch (opt_kind)
            {
                case(TCPOPT_EOL):
                    break; // End of options list

                case(TCPOPT_NOP):
                    break; // No operation

                case(TCPOPT_MAXSEG):
                    if (opt_size == TCPOLEN_MAXSEG)
                    {
                        uint16_t mss;
                        memcpy(&mss, opt_ptr + 2, 2);
                        printf("    - MSS: %u\n", ntohs(mss));
                    }
                    break; // Maximum Segment Size

                case(TCPOPT_WINDOW):
                    break; // Window scale

                case(TCPOPT_SACK_PERMITTED):
                    break; // Selective ACK (SACK) permitted

                case(TCPOPT_SACK):
                    break; // SACK

                case(TCPOPT_TIMESTAMP):
                    if (opt_size == TCPOLEN_TIMESTAMP)
                    { // len should be 10
                        uint32_t ts_val, ts_ecr;
                        memcpy(&ts_val, opt_ptr + 2, 4);
                        memcpy(&ts_ecr, opt_ptr + 6, 4);

                        double ts_val_sec = ntohl(ts_val) / 1000.0; // convert to seconds
                        double ts_ecr_sec = ntohl(ts_val) / 1000.0;
            
                        printf("    - Timestamps   : TSval = %.3f, TSecr = %.3f\n", ts_val_sec, ts_ecr_sec);
                    }
                    break;

                default:
                    printf("    - Unknown Option   : Type = %u, Length = %u\n", opt_kind, opt_size);
            }
            opt_ptr += opt_size;
            remaining -= opt_size;
        }
    }

    /* Print the payload if there is one */
    if (payload_len > 0)
    {
        if (payload != NULL)
        {
            printf("  Payload Data     : ");

            size_t display_len = (payload_len > 64) ? 64 : payload_len; // Limit characters to 64
            print_safe_chars(payload, display_len);
        
            if (payload_len > 64)
                printf("                     [... truncated ...]\n");
        }

    }
    printf("----------------------------------------\n\n");
    free(local_buf);
}


void print_tcb(const tcb_t *tcb)
{
    if (is_null(tcb, "TCB: (null)"))
        return;

    double ts_rcv_val_sec = tcb->ts_rcv_val / 1000.0; // convert to seconds

    printf("\n~~~~~~~~~~~~~~~~~~~TCB~~~~~~~~~~~~~~~~~~");
    print_fourtuple(&tcb->fourtuple);
    printf("\n");

    printf("  dest_udp_port     : %u\n", tcb->dest_udp_port);
    printf("  fsm_state         : %s\n", fsm_state_to_str(tcb->fsm_state));
    printf("  file descriptor   : %u\n", tcb->fd);

    printf("\n");

    printf("  iss               : %u\n", tcb->iss);
    printf("  snd_una           : %u\n", tcb->snd_una);
    printf("  snd_nxt           : %u\n", tcb->snd_nxt);
    printf("  snd_wnd           : %u\n", tcb->snd_wnd);
    printf("  irs               : %u\n", tcb->irs);
    printf("  rcv_nxt           : %u\n", tcb->rcv_nxt);
    printf("  rcv_wnd           : %u\n", tcb->rcv_wnd);
    
    printf("\n");

    printf("  ts_rcv_val        : %.3f\n", ts_rcv_val_sec);
    printf("  rto               : %u\n", tcb->rto);
    printf("  srtt              : %u\n", tcb->srtt);
    printf("  rttvar            : %u\n", tcb->rttvar);
    printf("  snd_cwnd          : %u\n", tcb->snd_cwnd);
    printf("  snd_ssthresh      : %u\n", tcb->snd_ssthresh);
    printf("  dupacks           : %u\n", tcb->dupacks);
    printf("  rxtcur            : %u\n", tcb->rxtcur);
    printf("  rttmin            : %u\n", tcb->rttmin);
    printf("  tcpt_rexmt        : %u\n", tcb->tcpt_rexmt);
    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n\n");
}

void print_fourtuple(const fourtuple_t *tup)
{
    if (is_null(tup, "Four Tuple: (null)"))
        return;

    printf("\n  Four Tuple:\n");
    printf("    source (UTCP) port:    %u\n", tup->source_port);
    printf("    source ip:    %u\n", tup->source_ip);
    printf("    dest (UTCP) port:    %u\n", tup->dest_port);
    printf("    dest ip:    %u\n", tup->dest_ip);
}

void print_safe_chars(uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        char c = (char)buf[i];
        printf("%c", isprint(c) ? c : '.'); // non-printable bytes → '.'
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
        printf("current TCB:\n");
        print_tcb(global->tcb_lookup[i]);
    }
}