/**
 * A simple struct that represents a full TCP
 * segement, which is a TCP header, and the
 * associated data payload.
 */
#include <stdint.h>
#include <tcphdr.h>

#ifndef TCP_SEGMENT_H
#define TCP_SEGMENT_H

struct tcp_segment {
    tcphdr hdr;
    uint8_t data[];
};
#endif