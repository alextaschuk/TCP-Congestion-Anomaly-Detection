#include <utcp/api/tx_dgram.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/types.h>

#include <tcp/tcp_segment.h>
#include <utils/printable.h>
#include <utils/logger.h>
#include <utils/err.h>
#include <utcp/api/conn.h>
#include <utcp/api/globals.h>


uint8_t tcp_outflags[] = {
    TH_RST | TH_ACK, 0,      TH_SYN, TH_SYN | TH_ACK, TH_ACK, TH_ACK, TH_FIN | TH_ACK, TH_FIN | TH_ACK,
    TH_FIN | TH_ACK, TH_ACK, TH_ACK,
};


int retransmit_data(tcb_t *tcb, uint32_t seq)
{
    LOG_DEBUG("[retransmit_data] retransmission requested. target sequence number=%u", seq);
    if(tcb->fsm_state != ESTABLISHED)
        return 0;

    uint8_t flags = tcp_outflags[tcb->fsm_state];
    
    /**
     * Option length for timestamps in bytes. 10 bytes for the timestamp & 8 bits bytes for NOP padding.
     * We need 2 bytes of padding because the total size of the timestamp + header is 30 bytes (w/out)
     * padding, which is not divisible by 4. 
     */
    size_t opt_len = 12;


    /* How much data in the buffer corresponds to this seq number? */
    uint32_t data_bytes_sent = (seq > tcb->iss) ? (seq - tcb->iss - 1) : 0;
    uint32_t buffered_data = 0;

    if (tcb->tx_tail > data_bytes_sent)
        buffered_data = tcb->tx_tail - data_bytes_sent;

    size_t send_len = MIN(buffered_data, (size_t)MSS); // Send max 1 MSS of data
    LOG_DEBUG("[retransmit_data] calculations for retransmission: bytes_sent=%u, buffered_data=%u, taking %u bytes.", data_bytes_sent,
                buffered_data, send_len);

    int bytes_sent = send_segment(tcb, seq, send_len, opt_len, flags);
    //tcb->snd_nxt += send_len;
    
    log_tcb(tcb, "[retransmit_data] retransmit post-send:");
    return  bytes_sent;
}


static int send_segment(tcb_t *tcb, uint32_t seq, size_t data_len, size_t opt_len, uint8_t flags)
{
    /* Allocate for a segment (TCP header + options + payload) */
    size_t segment_size = sizeof(struct tcphdr) + opt_len + data_len;
    tcp_segment_t *segment = malloc(segment_size);
    if (!segment)
        LOG_ERROR("[send_dgram] error allocating segment");

    /* Initialize the segment's base header */
    memset(&segment->hdr, 0, sizeof(struct tcphdr));

    segment->hdr.th_sport = htons(tcb->fourtuple.source_port);
    segment->hdr.th_dport = htons(tcb->fourtuple.dest_port);
    segment->hdr.th_seq = htonl(seq);
    segment->hdr.th_ack = htonl(tcb->rcv_nxt);
    segment->hdr.th_off = (sizeof(struct tcphdr) + opt_len) / 4; // convert into 32-bit words
    segment->hdr.th_flags = flags;

    /* calculate available space and clamp to header */
    uint32_t bytes_in_buffer = BUF_SIZE - (tcb->rx_tail - tcb->rx_head);
    uint32_t current_free_space = BUF_SIZE - bytes_in_buffer - 1;
    //uint32_t current_free_space = MIN(bytes_in_buffer, BUF_SIZE);

    if (flags & TH_SYN)
        segment->hdr.th_win = htons(SET_SCALED_WIN(tcb, flags, current_free_space));  

    /* Add options */
    uint8_t opt_buf[40]; // max length of options is 40 bytes
    int opt_idx = 0;

    /* timestamps option (12 bytes total: 2 NOP + 10 byte TS option) */
    opt_buf[opt_idx++] = 1; // NOP (TCPOPT_NOP)
    opt_buf[opt_idx++] = 1; // NOP (TCPOPT_NOP)
    opt_buf[opt_idx++] = 8; // Option-Kind = timestamp & previous timestamp's echo (TCPOPT_TIMESTAMP)
    opt_buf[opt_idx++] = 10; // Option-Length (TCPOLEN_TIMESTAMP)

    uint32_t raw_val = htonl(tcp_now()); // update timestamp
    uint32_t raw_ecr = htonl(tcb->ts_rcv_val); // echo the peer's TSval

    memcpy(&opt_buf[opt_idx], &raw_val, 4); // TSval
    opt_idx += 4;
    memcpy(&opt_buf[opt_idx], &raw_ecr, 4); // TSecr
    opt_idx += 4;

    /* window scale option (4 bytes total: 1 NOP + 3 byte WS option) */
    if (flags & TH_SYN)
    {
        opt_buf[opt_idx++] = 1; // NOP (TCPOPT_NOP)   
        opt_buf[opt_idx++] = 3; // Option-Kind = Window Scale (TCPOPT_WINDOW)
        opt_buf[opt_idx++] = 3; // Option-Length (TCPOLEN_WINDOW)

        // This is the shift count the peer should use when reading the advertised 
        // window. This is set in alloc_tcb (e.g. 4 for a 2^4 = 16x markiplier)
        opt_buf[opt_idx++] = tcb->rcv_ws_scale; 
    }

    if (opt_idx != opt_len)
        LOG_WARN("[send_segment] OPTION LENGTH MISMATCH: Wrote %d bytes, expected %zu", opt_idx, opt_len);

    memcpy(segment->data, opt_buf, opt_len);

    /* Add buffer to the payload and send the segment */
    if (data_len > 0)
    {
        uint32_t buf_offset = (seq - tcb->iss - 1) % BUF_SIZE;

        if (buf_offset + data_len <= BUF_SIZE)
        {   // offset pointer by opt_len so that timestamps arent overwritten
            memcpy(segment->data + opt_len, &tcb->tx_buf[buf_offset], data_len); 
        }
        else 
        {
            // buffer wraps around, so it needs to be split into 2 parts
            size_t part1_len = BUF_SIZE - buf_offset;
            size_t part2_len = data_len - part1_len;
            
            LOG_DEBUG("[send_segment] Data wraps around ring buffer. Copying %zu bytes from end, %zu bytes from start.", part1_len, part2_len);
            
            memcpy(segment->data + opt_len, &tcb->tx_buf[buf_offset], part1_len);
            memcpy(segment->data + opt_len + part1_len, &tcb->tx_buf[0], part2_len);
        }
    }

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(tcb->dest_udp_port);
    dest_addr.sin_addr.s_addr = htonl(tcb->fourtuple.dest_ip);

    log_segment((u_int8_t *)segment, segment_size, 0, "[send_dgram] Segment that was sent:");

    ssize_t bytes_sent = sendto(tcb->src_udp_fd, segment, segment_size, 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (bytes_sent < 0)
        err_sys("[send_dgram] Error sending packet");

    free(segment);
    return bytes_sent;
}


int send_dgram(tcb_t *tcb)
{
    uint8_t flags = tcp_outflags[tcb->fsm_state];
    bool force_ack = false;

    LOG_DEBUG("[send_dgram] Initial state: %s, Base Flags: 0x%02X", fsm_state_to_str(tcb->fsm_state), flags);

    /**
     * Base option length for timestamps in bytes. 10 bytes for the timestamp & 2 bytes for NOP padding
     * We need 2 bytes of padding because options must fit into 4-byte boundaries.
     */
    size_t opt_len = 12;

    if (flags & TH_SYN)
        opt_len += 4;

    if (tcb->t_flags & F_ACKNOW)
    { // an ACK needs to be immediately sent.
        force_ack = true;
        LOG_DEBUG("[send_dgram] F_ACKNOW flag is up in the TCB. Sending a forced ACK.");
        tcb->t_flags &= ~F_ACKNOW; // put the flag down
    }

    int total_bytes_sent = 0;
    int segments_sent = 0;

    while(1)
    {
        size_t data_len = 0;

        if (tcb->fsm_state == ESTABLISHED)
        {
            /* Calculate how much data can be sent according to the receiver's window and the cwnd */
            uint32_t send_window = MIN(tcb->snd_wnd, tcb->cwnd); // (effective window)
            uint32_t unacked_bytes_in_flight = tcb->snd_nxt - tcb->snd_una; // bytes that have been sent (in flight) (on the wire)
            //LOG_INFO("[send_dgram] Send Window: %u, unACKed bytes in flight: %u", send_window, unacked_bytes_in_flight);
        
            /**
             * Calculate the total number of payload bytes that have been sent during the connection's entire session.
             * This includes the sum of payload bytes - 1 (for the SYN flag), or 0 if only a SYN request has been made.
             */
            uint32_t data_bytes_sent = SEQ_GT(tcb->snd_nxt, tcb->iss) ? (tcb->snd_nxt - tcb->iss - 1) : 0;

            uint32_t buffered_bytes = 0; // The number of bytes the application has written to TX and are waiting to be sent.
            if (tcb->tx_tail > data_bytes_sent)
                buffered_bytes = tcb->tx_tail - data_bytes_sent;

            LOG_DEBUG("[send_dgram] Window Calculation: Used snd_wnd=%u, cwnd=%u, to calculate effective wnd=%u, bytes in flight=%u, bytes buffered=%u",
                        tcb->snd_wnd, tcb->cwnd, send_window, unacked_bytes_in_flight, buffered_bytes);

            /* How many bytes can we fit into the current segment? */
            if (send_window > unacked_bytes_in_flight)
            {
                uint32_t can_send = send_window - unacked_bytes_in_flight;
                data_len = MIN(MIN(buffered_bytes, can_send), MSS); // don't send more bytes than the max segment size

                /**
                 * Nagle's Algorithm: suppress tiny segments when there is already
                 * unacknowledged data in flight.  Only send a sub-MSS segment if the
                 * pipe is completely empty (no in-flight data), which means we have
                 * reached the tail of the send buffer.  This prevents the sender-side
                 * silly-window syndrome where every small ACK triggers an equally small
                 * new data segment.
                 */
                if (data_len > 0 && data_len < MSS && unacked_bytes_in_flight > 0) {
                    LOG_DEBUG("Nagle: suppressing %zu-byte segment (InFlight=%u). "
                                "Waiting for full MSS or pipe drain.",
                                data_len, unacked_bytes_in_flight);
                    data_len = 0;
                }

                if (data_len > 0) {
                    //LOG_INFO("Preparing to send %zu bytes of payload.", data_len);
                }

            }
            else
            {
                data_len = 0;
                //LOG_DEBUG("[send_dgram] The send window is full or is being blocked by the cwnd. snd_wnd=%u, cwnd=%u, bytes in flight=%u",
                //           tcb->snd_wnd, tcb->cwnd, unacked_bytes_in_flight);
            }
        }

            bool is_syn_fin = (flags & (TH_SYN | TH_FIN)) != 0; // is the packet a SYN or a FIN request?
            bool sending_new_syn_fin = is_syn_fin && (tcb->snd_nxt == tcb->snd_max); // is the packet a duplicate SYN or FIN?

            LOG_INFO("[send_dgram] snd_next=%u, snd_max=%u, is_syn_fin:%d sending_new_syn_fin=%d", tcb->snd_nxt, tcb->snd_max, is_syn_fin, sending_new_syn_fin);

            if (data_len == 0 && !sending_new_syn_fin && !force_ack)
            { // no payload and it's not a SYN or FIN, so no point in sending the packet
                LOG_WARN("[send_dgram] Segment has empty payload and isn't a SYN or FIN. Ignoring request and exiting loop. data_len=%zu is_syn_fin=%i", data_len, is_syn_fin);
                break;
            }
        
            LOG_INFO("[send_dgram] Sending datagram to UTCP port [%u], UDP port [%u]. Payload size: %zu bytes",  tcb->fourtuple.dest_port, tcb->dest_udp_port, data_len);

            int bytes_sent = send_segment(tcb, tcb->snd_nxt, data_len, opt_len, flags);
            if (bytes_sent < 0)
            {
                LOG_ERROR("[send_dgram] Error occurred in send_segment(). Exiting loop.");
                break;
            }

            total_bytes_sent += bytes_sent;
            segments_sent++;

            if (data_len > 0 || sending_new_syn_fin)
            { // only increase snd_nxt if payload isn't empty or a SYN/FIN is being sent.
                uint32_t consumed = data_len + (sending_new_syn_fin ? 1 : 0);
                tcb->snd_nxt += consumed;

                if(tcb->t_timer[TCPT_REXMT] == 0)
                { // start the retransmission timer
                    int ticks = reset_timer(tcb, TCPT_REXMT);
                    LOG_DEBUG("[send_dgram] REXMT timer counting down from %d ticks (%u ms)", ticks, tcb->rto);
                }
    
                LOG_DEBUG("[send_dgram] snd_nxt advanced by %u bytes. New snd_nxt=%u", consumed, tcb->snd_nxt);
            }

            if (SEQ_GT(tcb->snd_nxt, tcb->snd_max))
            {
                tcb->snd_max = tcb->snd_nxt;
                LOG_DEBUG("[send_dgram] Advanced snd_max to %u", tcb->snd_max);
            }

            force_ack = false; // We fulfilled the force_send requirement on the first pass, don't loop it

            if (data_len == 0)
            { // the segment that was sent was either an empty ACK, a SYN, or a FIN, so we can exit the loop.
                break;
            }
    }

    if (segments_sent > 0) 
        LOG_DEBUG("[send_dgram] Finished sending data in TX. Burst %d segments, %d total bytes.", segments_sent, total_bytes_sent);

    return total_bytes_sent;
}
