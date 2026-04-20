#include <utils/logger.h>

#include <stdbool.h>
#include <stdint.h>

#include <utcp/api/utcp_timers.h>

/* Clamp ssthresh to avoid the enormous initial value (0xFFFFFFFF) swamping the CSV */
#define SSTHRESH_CAP 1050000U /* 750 * MSS */

int init_zlog(char conf_path[])
{
    /* initialize zlog w/ zlog.conf */
    int rc = dzlog_init(conf_path, "UTCP");
    
    if (rc)
    {
        printf("zlog init failed\n");
        return -1;
    }
    return 0;
}


void log_lstm_event(struct tcb_t *tcb_t, uint32_t rtt_us, uint32_t newly_acked, bool is_dup_ack, bool is_timeout)
{
    uint64_t now_us = utcp_ticker();

    /* Inter-event time (0 for the very first event on this connection) */
    uint64_t inter_ack_us = (tcb_t->lstm_last_ts_us > 0) ? (now_us - tcb_t->lstm_last_ts_us) : 0;
    tcb_t->lstm_last_ts_us  = now_us;

    /* Convert tick-scaled tcb_t fields to raw microseconds */
    uint32_t srtt_us   = (tcb_t->srtt   >> 3) * (uint32_t)TCP_TICK_MS * 1000U;
    uint32_t rttvar_us = (tcb_t->rttvar >> 2) * (uint32_t)TCP_TICK_MS * 1000U;
    uint32_t rto_us    = tcb_t->rxtcur  * (uint32_t)TCP_TICK_MS * 1000U;
    uint64_t min_rtt   = tcb_t->min_rtt_seen_us;

    /*
     * Queuing delay: how far above the observed minimum we are right now.
     * Only meaningful when we have a real RTT sample and an established baseline.
     */
    int64_t queue_delay_us = 0;
    if (rtt_us > 0 && min_rtt > 0 && (uint64_t)rtt_us > min_rtt)
        queue_delay_us = (int64_t)rtt_us - (int64_t)min_rtt;

    /* RTT first derivative (latency trend) */
    int32_t rtt_delta_us = 0;
    if (rtt_us > 0 && tcb_t->lstm_prev_rtt_us > 0)
        rtt_delta_us = (int32_t)rtt_us - (int32_t)tcb_t->lstm_prev_rtt_us;

    /* RTT second derivative (is the trend accelerating?) */
    int32_t rtt_accel_us = rtt_delta_us - tcb_t->lstm_prev_rtt_delta_us;

    /* RTO first derivative (captures combined SRTT + RTTVAR growth) */
    int32_t rto_delta_us = (int32_t)rto_us - (int32_t)tcb_t->lstm_prev_rto_us;

    /* Advance per-connection tracking state for next call */
    if (rtt_us > 0)
    {
        tcb_t->lstm_prev_rtt_delta_us = rtt_delta_us;
        tcb_t->lstm_prev_rtt_us = rtt_us;
    }

    tcb_t->lstm_prev_rto_us = rto_us;

    /* For timeout rows, snd_nxt has NOT yet been rolled back (caller guarantees this),
     * so flight_size correctly reflects the data in-flight at the moment of loss. */
    uint32_t flight_size  = tcb_t->snd_nxt - tcb_t->snd_una;
    uint32_t ssthresh_log = (tcb_t->ssthresh > SSTHRESH_CAP) ? SSTHRESH_CAP : tcb_t->ssthresh;

    const char *old_category = current_thread_cat;
    current_thread_cat = "lstm_logger";
    LOG_INFO(/* timestamp_us, rtt_us, srtt_us, rttvar_us, rto_us, min_rtt_us */
                    "%llu,      %u,     %u,         %u,     %u,     %llu,"
              /* queue_delay_us, rtt_delta_us, rtt_accel_us, rto_delta_us */
                    "%lld,             %d,          %d,             %d,"
              /* cwnd, ssthresh, snd_wnd, flight_size, newly_acked */
                  "%u,     %u,      %u,         %u,         %u,"
              /* inter_ack_us, ca_state, dupacks, rxtshift, is_dup_ack, is_timeout */
                    "%llu,         %u,      %u,       %u,       %u,         %u",
              (unsigned long long)now_us,
              rtt_us, srtt_us, rttvar_us, rto_us,
              (unsigned long long)min_rtt,
              (long long)queue_delay_us, rtt_delta_us, rtt_accel_us, rto_delta_us,
              tcb_t->cwnd, ssthresh_log, tcb_t->snd_wnd, flight_size, newly_acked,
              (unsigned long long)inter_ack_us,
              (uint32_t)tcb_t->ca_state, (uint32_t)tcb_t->dupacks, (uint32_t)tcb_t->rxtshift,
              (uint32_t)is_dup_ack, (uint32_t)is_timeout);

    current_thread_cat = old_category;
}