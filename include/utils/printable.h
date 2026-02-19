#ifndef PRINTABLE_H
#define PRINTABLE_H

#include <stdbool.h>
#include <netinet/tcp.h>

#include <tcp/tcb.h>
#include <tcp/fourtuple.h>

/* Begin function declaration */

/**
 * @brief helper function to determine if the struct
 * to be printed is null or not.
 */
static bool is_null(const void *ptr, const char *msg);

/**
 * @brief prints the contents of a segment in a
 * readable format. The TCP header is printed, then
 * followed by the segment's data if there is any.
 * 
 * @param *buf a buffer of data that is received via
 * `recvfrom` or sent via `sendto`.
 * @param buflen the length in bytes of the segment
 * @param flow `0` if packet is being sent (outgoing),
 * `1` if packet is being received (incoming).
 */
void print_segment(const uint8_t *buf, const size_t buflen, const bool flow);

void print_tcb(const tcb_t *tcb);


void print_fourtuple(const fourtuple *tup);

/* End function declaration */
#endif
