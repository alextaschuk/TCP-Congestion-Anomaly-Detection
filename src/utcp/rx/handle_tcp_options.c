#include <utcp/rx/handle_tcp_options.h>

#include <stdio.h>
#include <string.h>

#include <utils/err.h>
#include <utils/logger.h>
#include <utils/printable.h>
#include <utcp/api/globals.h>


bool parse_tcp_options(struct tcphdr *hdr, parsed_tcp_opts_t *opts)
{
    memset(opts, 0, sizeof(parsed_tcp_opts_t));

    int opt_len = (hdr->th_off * 4) - sizeof(struct tcphdr);
    uint8_t *opt_ptr = (uint8_t *)hdr + sizeof(struct tcphdr); // the 1st option byte right after the TCP header

    while (opt_len > 0) 
    { // iterate over the options array (ts_opt in send_dgram())
        uint8_t opt_kind = opt_ptr[0];

        /* check for single-byte options (only have the "kind" byte, no length or value bytes) */
        if (opt_kind == TCPOPT_EOL) // End of options list
            break;
        
        if (opt_kind == TCPOPT_NOP) // No operation
        {
            opt_ptr++;
            opt_len--;
            continue;
        }

        if (opt_len < 2)
        {
            LOG_WARN("[find_timestamps] Malformed options: truncated before length byte");
            break;
        }

        /* check for multi-byte options that have length & value bytes*/
        uint8_t opt_size = opt_ptr[1];

        if (opt_size < 2 || opt_size > opt_len)
        {
            LOG_WARN("[find_timestamps] Malformed options: invalid length byte (%u), stopping parse.", opt_size);
            break;
        }
            
        switch (opt_kind)
        {
            case(TCPOPT_MAXSEG): // Maximum Segment Size
                break; 

            case(TCPOPT_WINDOW): // Window scale
                if (opt_size == TCPOLEN_WINDOW)
                {
                    opts->has_ws = true;
                    opts->ws_val = opt_ptr[2]; // shift count is 3rd byte (idx 2)

                    // Max window scale is 14 (See RFC 7323, Section 2.2)
                    opts->ws_val = MIN(opts->ws_val, 14);
                }
                break;

            case(TCPOPT_SACK_PERMITTED): // Selective ACK (SACK) permitted
                break;

            case(TCPOPT_SACK): // SACK
                break;

            case(TCPOPT_TIMESTAMP): // Timestamp
                if (opt_size == TCPOLEN_TIMESTAMP)
                {
                uint32_t raw_val, raw_ecr;
                memcpy(&raw_val, opt_ptr + 2, 4);
                memcpy(&raw_ecr, opt_ptr + 6, 4);

                opts->has_ts = true; // we have timestamps!
                opts->ts_val = ntohl(raw_val);
                opts->ts_ecr = ntohl(raw_ecr);
                //LOG_INFO("[find_timestamps] Found timestamps in header. TSval: %u, TSecr: %u", ntohl(raw_val), ntohl(raw_ecr));
                }
                break;

            default:
                break;
        }

        opt_ptr += opt_size;
        opt_len -= opt_size;
    }
    return opts->has_ts;
}


void process_tcp_options(tcb_t *tcb, struct tcphdr *hdr, bool is_syn)
{
    parsed_tcp_opts_t opts;
    parse_tcp_options(hdr, &opts);

    if (opts.has_ts)
    {
        /* PAWS (See RFC 1323, Section 4): An ACK may contain a payload of data that is
           out of order, so we only want to update the peer's timestamp if it is in order. */
        if (SEQ_LEQ(hdr->th_seq, tcb->rcv_nxt))
            tcb->ts_rcv_val = opts.ts_val;

        if (hdr->th_flags & TH_ACK) // we only calculate RTT if the packet is ACKing something
            calc_rto(tcb, opts.ts_ecr);
    }
    else if (!is_syn)
        LOG_WARN("[process_tcp_options] TCP header is missing timestamps. Skipping RTT update");

    if (is_syn && opts.has_ws)
    {
        tcb->snd_ws_scale = opts.ws_val;
        tcb->ws_enabled = true;
        tcb->snd_wnd = GET_SCALED_WIN(tcb, hdr);

        //LOG_INFO("[process_tcp_options] Peer supports Window Scale. Send shift count: %u", tcb->snd_ws_scale);
    }
}