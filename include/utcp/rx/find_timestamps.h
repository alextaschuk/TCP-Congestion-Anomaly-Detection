#ifndef FIND_TIMESTAMPS_H
#define FIND_TIMESTAMPS_H

#include <stdbool.h>
#include <stdint.h>

#include <netinet/tcp.h>


/**
 * Search for timestamps in the Options section of a TCP header. Since this section can have a variable
 * length of up to 40 bytes, we need to see how many bytes of options exist so that we don't read into
 * the payload.
 * 
 * @param *hdr The received TCP header that we are searching through.
 * @param *ts_val If/when TSval is found, it is stored here.
 * @param *ts_ecr If/when TSecr is found, it is stored here.
 * 
 * @returns `true` if valid TSval and TSecrl values are found, `false` otherwise.
 */
bool find_timestamps(struct tcphdr *hdr, uint32_t *ts_val, uint32_t *ts_ecr);


#endif
