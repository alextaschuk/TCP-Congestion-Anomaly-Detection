#ifndef HANDLE_TCP_OPTIONS_H
#define HANDLE_TCP_OPTIONS_H

#include <stdbool.h>
#include <stdint.h>

#include <netinet/tcp.h>

#include <tcp/tcb.h>

#ifndef TCPOPT_WINDOW
#define TCPOPT_WINDOW 3
#endif

#ifndef TCPOLEN_WINDOW
#define TCPOLEN_WINDOW 3
#endif

/**
 * Contains whatever options we find in the header.
 * Currently only supports the window scale and timestamps option.
 */
typedef struct parsed_tcp_opts_t{
    bool has_ts;
    uint32_t ts_val;
    uint32_t ts_ecr;
    
    bool has_ws;
    uint8_t ws_val; // Window scale shift count
} parsed_tcp_opts_t;


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
bool parse_tcp_options(struct tcphdr *hdr, parsed_tcp_opts_t *opts);


/**
 * Check for the options in the TCP header. 
 */
void process_tcp_options(tcb_t *tcb, struct tcphdr *hdr, bool is_syn);


#endif
