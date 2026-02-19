#include <utils/printable.h>

#include <arpa/inet.h> 
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <utils/err.h>
#include <tcp/tcp_segment.h>
#include <utcp/api/api.h>

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

    // this function should be non-destructive, so we need a local copy of the data
    uint8_t *local_buf = malloc(buflen);
    if (!local_buf)
        err_sys("[print_segment] malloc for local_buf failed.\n");
    memcpy(local_buf, buf, buflen);

    struct tcphdr *hdr;
    uint8_t* data; // should be empty
    ssize_t data_len; // should be 0
    deserialize_utcp_packet(local_buf, buflen, &hdr, &data, &data_len);

    const char *direction = flow ? "<<< [INCOMING PACKET]" : "[OUTGOING PACKET] >>>";

    printf("%s\n"
           "  Src UTCP Port  : %u\n"
           "  Dest UTCP Port : %u\n"
           "  Sequence Number  : %u\n"
           "  Ack Number       : %u\n"
           "  Flags            : [ %s%s%s%s%s%s]\n"
           "  Window           : %u\n",
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
           hdr->th_win
        );

    // Only print payload info if a length is provided
    //if (data_len > 0) {
    //    printf("  Payload Length   : %zu bytes\n", data_len);
        //if (data != NULL) {
        //    printf("  Payload Data     : ");
        //    size_t display_len = (data_len > 64) ? 64 : data_len; // Limit characters to 64
        //    print_safe_chars(data, display_len);
        //
        //    if (data_len > 64) {
        //        printf("                     [... truncated ...]\n");
        //    }
        //}

    //}
    printf("----------------------------------------\n");
    free(local_buf);
}

void print_tcb(const tcb_t *tcb)
{
    if (is_null(tcb, "TCB: (null)"))
        return;

    printf("TCB {\n");
    print_fourtuple(&tcb->fourtuple);
    printf("\n");

    printf("  dest_udp_port: %u\n", tcb->dest_udp_port);
    printf("  fsm_state:     %u\n", tcb->fsm_state);

    printf("  iss:     %u\n", tcb->iss);
    printf("  snd_una: %u\n", tcb->snd_una);
    printf("  snd_nxt: %u\n", tcb->snd_nxt);
    printf("  rcv_nxt: %u\n", tcb->rcv_nxt);
    printf("  irs:     %u\n", tcb->irs);
    printf("  rcv_wnd: %u\n", tcb->rcv_wnd);

    printf("}\n");
}

void print_fourtuple(const fourtuple *tup)
{
    if (is_null(tup, "Four Tuple: (null)"))
        return;

    printf("\n  Four Tuple:\n");
    printf("    source (UTCP) port:    %u\n", tup->source_port);
    printf("    source ip:    %u\n", tup->source_ip);
    printf("    dest (UTCP) port:    %u\n", tup->dest_port);
    printf("    dest ip:    %u\n", tup->dest_ip);
}
