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

void print_tcphdr(const tcphdr *tcp)
{
    /**
     * @brief print the contents of a
     * TCP header that is in network
     * byte order
     */
    if (is_null(tcp, "TCP Header: (null)"))
        return;

    printf("\nTCP Header:\n");
    printf("  Source Port:      %u\n", ntohs(tcp->th_sport));
    printf("  Destination Port: %u\n", ntohs(tcp->th_dport));
    printf("  Sequence Number:  %u\n", ntohl(tcp->th_seq));
    printf("  Acknowledgment:   %u\n", ntohl(tcp->th_ack));
    printf("  Data Offset:      %u bytes\n", (tcp->th_off_flags >> 4) * 4);
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

void print_tcb(const tcb *tcb)
{
    return;
}

void print_fourtup(const fourtuple *tup)
{
    if (is_null(tup, "Four Tuple: (null)"))
        return;

    printf("\nFour Tuple:\n");
    printf("");
}
