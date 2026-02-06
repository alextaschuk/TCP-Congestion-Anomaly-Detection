/**
 * see https://sites.uclouvain.be/SystInfo/usr/include/netinet/tcp.h.html
 */

#include <stdint.h>

#ifndef TCPHDR_H
#define TCPHDR_H

typedef struct{
    uint16_t th_sport;    /* source port */
    uint16_t th_dport;    /* destination port */
    uint32_t th_seq;      /* sequence number */
    uint32_t th_ack;      /* acknowledgement number */
    uint8_t th_off_flags; /* upper 4 bits offset, lower 4 bits unused */
    uint8_t th_flags;
#define TH_FIN 0x01
#define TH_SYN 0x02
#define TH_RST 0x04
#define TH_PUSH 0x08
#define TH_ACK 0x10
#define TH_URG 0x20
    uint16_t th_win; /* window */
    uint16_t th_sum; /* checksum */
    uint16_t th_urp; /* urgent pointer */
}tcphdr;
#endif