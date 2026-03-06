#ifndef TCP_SEGMENT_H
#define TCP_SEGMENT_H

#include <stdint.h>

#include <netinet/tcp.h>


/**
 * A simple struct that represents a full TCP
 * segement, which is a TCP header, and the
 * associated data payload.
 */
typedef struct tcp_segment_t
{
    struct tcphdr hdr;
    uint8_t data[];
} tcp_segment_t;

#endif
