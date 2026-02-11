#include <stdio.h>
#include <stdint.h>
#include <arpa/inet.h> 

#include <utils/printable.h>

bool is_null(const void *ptr, const char *msg)
{
    if (ptr == NULL)
    {
        printf("%s\n", msg);
        return true;
    }
    return false;
        
}

void print_tcphdr(const struct tcphdr *tcp)
{
    if (is_null(tcp, "TCP Header: (null)"))
        return;

    printf("\nTCP Header:\n");
    printf("  Source Port:      %u\n", ntohs(tcp->th_sport));
    printf("  Destination Port: %u\n", ntohs(tcp->th_dport));
    printf("  Sequence Number:  %u\n", ntohl(tcp->th_seq));
    printf("  Acknowledgment:   %u\n", ntohl(tcp->th_ack));
    printf("  Data Offset:      %u bytes\n", (tcp->th_off >> 4) * 4);
    printf("  Flags:");
    if (tcp->th_flags & TH_FIN)  printf(" FIN");
    if (tcp->th_flags & TH_SYN)  printf(" SYN");
    if (tcp->th_flags & TH_RST)  printf(" RST");
    if (tcp->th_flags & TH_PUSH) printf(" PSH");
    if (tcp->th_flags & TH_ACK)  printf(" ACK");
    if (tcp->th_flags & TH_URG)  printf(" URG");
    printf("\n");
    printf("  Window Size:      %u\n", ntohs(tcp->th_win));
    printf("  Checksum:         0x%04x\n", ntohs(tcp->th_sum));
    printf("  Urgent Pointer:   %u\n\n", ntohs(tcp->th_urp));
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
    printf("    source port:    %u\n", tup->source_port);
    printf("    source ip:    %u\n", tup->source_ip);
    printf("    dest port:    %u\n", tup->dest_port);
    printf("    dest ip:    %u\n", tup->dest_ip);
}
