#include <utcp/rx/find_timestamps.h>

#include <stdio.h>
#include <string.h>

#include <utils/err.h>
#include <utils/logger.h>s
#include <utils/printable.h>
#include <utcp/api/globals.h>


bool find_timestamps(struct tcphdr *hdr, uint32_t *ts_val, uint32_t *ts_ecr)
{
    int opt_len = (hdr->th_off * 4) - sizeof(struct tcphdr);
    uint8_t *opt_ptr = (uint8_t *)hdr + sizeof(struct tcphdr); // the 1st option byte right after the TCP header

    while (opt_len > 0) 
    { // iterate over the options array (ts_opt in send_dgram())
        uint8_t opt_kind = opt_ptr[0];

        /* check for single-byte options (only have the kind byte, no length or value bytes) */
        if (opt_kind == TCPOPT_EOL) // End of options list
            break;
        
        if (opt_kind == TCPOPT_NOP) // No operation
        {
            opt_ptr++;
            opt_len--;
            continue;
        }

        if (opt_len < 2)
            err_data("[find_timestamps] Malformed options: cut off before length byte");

        /* check for multi-byte options that have length & value bytes*/
        uint8_t opt_size = opt_ptr[1];
        
        if (opt_size < 2 || opt_size > opt_len)
        {
            err_data("[find_timestamps] Malformed options: missing or invalid length byte.\n");
        }
            
        switch (opt_kind)
        {
            case(TCPOPT_MAXSEG): // Maximum Segment Size
                break; 

            case(TCPOPT_WINDOW): // Window scale
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

                *ts_val = ntohl(raw_val);
                *ts_ecr = ntohl(raw_ecr);
                LOG_INFO("[find_timestamps] Found timestamps in header. TSval: %u, TSecr: %u", ntohl(raw_val), ntohl(raw_ecr));

                return true;
                }
                break;

            default:
                break;
        }

        opt_ptr += opt_size;
        opt_len -= opt_size;
    }
    return false;
}
