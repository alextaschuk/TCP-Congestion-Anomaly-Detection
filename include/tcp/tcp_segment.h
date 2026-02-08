/**
 * A simple struct that represents a full TCP
 * segement, which is a TCP header, and the
 * associated data payload.
 */

#ifndef TCP_SEGMENT_H
#define TCP_SEGMENT_H

#include <stdint.h>
#include <tcp/tcphdr.h>

typedef struct tcp_segment {
    tcphdr hdr;
    uint8_t data[];
}tcp_segment;
#endif
