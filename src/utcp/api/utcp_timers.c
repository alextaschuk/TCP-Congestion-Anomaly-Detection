#include <utcp/api/utcp_timers.h>

#include <stdio.h>
#include <time.h>

#include <pthread.h>

#include <tcp/hndshk_fsm.h>
#include <utils/err.h>
#include <utils/logger.h>
#include <utcp/api/globals.h>
#include <utcp/api/tx_dgram.h>

const int tcp_backoff[MAXRXTSHIFT + 1] = {1, 2, 4, 8, 16, 32, 64, 64, 64, 64, 64, 64, 64};

void* utcp_ticker_thread(void) 
{
    current_thread_cat = "ticker_thread";
    LOG_INFO("[utcp_ticker_thread] UTCP 1ms slow timer started...");

    struct timespec ts;
    ts.tv_sec = 0; // 0 seconds
    ts.tv_nsec = ts.tv_nsec = TCP_TICK_MS * 1000000L; // 1000000L = 1,000,000 nanoseconds
    while (1) 
    {
        nanosleep(&ts, NULL); // sleep for 10ms
        utcp_slowtimo(); // wake up and process all TCP timers
    }
}


void utcp_slowtimo(void) 
{
    api_t *global = api_instance();

    //LOG_INFO("[utcp_slowtimo] Locking the lookup lock...");
    pthread_mutex_lock(&global->lookup_lock);

    for (int i = 0; i < MAX_CONNECTIONS; i++) 
    {
        tcb_t *tcb = global->tcb_lookup[i];
        if (!tcb)
            continue; 

        //LOG_INFO("[utcp_slowtimo] Locking the TCB...");
        pthread_mutex_lock(&tcb->lock);

        for (int timer_idx = 0; timer_idx < TCPT_NTIMERS; timer_idx++) 
        {
            if (tcb->t_timer[timer_idx] > 0) 
            {
                tcb->t_timer[timer_idx]--;
                //LOG_DEBUG("[utcp_slowtimo] Updated timer %i. New value is %hu", timer_idx, tcb->t_timer[timer_idx]);

                if (tcb->t_timer[timer_idx] == 0)
                {
                    //LOG_DEBUG("[utcp_slowtimo] Timer %i expired.", timer_idx);
                    utcp_timeout(tcb, timer_idx);
                }
            }
        }
        //LOG_INFO("[utcp_slowtimo] Unlocking the TCB lock...");
        pthread_mutex_unlock(&tcb->lock);
    }

    //LOG_INFO("[utcp_slowtimo] Unlocking the lookup lock...");
    pthread_mutex_unlock(&global->lookup_lock);
}


uint32_t tcp_now(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) !=0)
        err_sys("[tcp_now] Error getting the time");

    return (uint32_t)((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000));
}


void calc_rto(tcb_t *tcb, uint32_t segment_ts_ecr)
{
    uint32_t current_time = tcp_now();
    uint32_t rtt_sample = current_time - segment_ts_ecr; // This is R, or R'
    //LOG_INFO("[calc_rto] Current Time: %u, R / R': %u ms", current_time, rtt_sample);

    /* Calculate RTT with Jacobson/Karels Algorithm and set/update the RTO */
    if (tcb->srtt == 0)
    { // first RTT measurement R
        tcb->srtt = rtt_sample << 3;   // SRTT = RTT * 8
        tcb->rttvar = rtt_sample << 1; // RTTVAR = RTT / 2 * 4
        //LOG_INFO("[calc_rto] Calulated R. srtt = %u, rttvar = %u", tcb->srtt, tcb->rttvar);
    } else
    { // subsequent RTT measurement R'
        int32_t delta = rtt_sample - (tcb->srtt >> 3); // delta = ((R' - SRTT) / 8)

        tcb->srtt += delta; // SRTT_new = SRTT_old + (delta / 8)

        if (delta < 0) delta = -delta; // compute |delta|
        tcb->rttvar += delta - (tcb->rttvar >> 2); // RTTVAR_new = RTTVAR_old + (|delta| - RTTVAR_old) / 4
        //LOG_INFO("[calc_rto] Calculated R'. delta = %u, srtt = %u, rttvar = %u", delta, tcb->srtt, tcb->rttvar);
    }

    /* Compute the RTO in milliseconds */
    uint32_t current_srtt = tcb->srtt >> 3;
    uint32_t four_rttvar = tcb->rttvar;
    //LOG_INFO("[calc_rto] Calculating the RTO. The current srtt = %u, rttvar = %u", current_srtt, four_rttvar);

    uint32_t calculated_rto_ms = current_srtt + MAX(CLOCK_GRANULARITY, four_rttvar);

    tcb->rxtcur = MS_TO_TICKS(calculated_rto_ms); // convert to ticks for bounds checking
    //LOG_INFO("[calc_rto] RTO = srtt + MAX(CLOCK_GRANULARITY, four_rttvar) = %u + %u = %u", current_srtt, MAX(CLOCK_GRANULARITY, four_rttvar), tcb->rxtcur);

    /**
     * Enforce bounds (e.g., Min 200ms, Max 64 seconds)
     * @note Strict RFC 6298 says min should be 1000ms, but Linux uses 200ms.
     */
    if (tcb->rxtcur < TCPTV_MIN)
    {
        LOG_DEBUG("[calc_rto] RTO %u ticks clamped up to TCPTV_MIN=%d ticks.", tcb->rxtcur, TCPTV_MIN);   
        tcb->rxtcur = TCPTV_MIN;
    }
    else if (tcb->rxtcur > TCPTV_REXMTMAX)
    {
        LOG_DEBUG("[calc_rto] RTO %u ticks clamped up to TCPTV_REXMTMAX=%d ticks.", tcb->rxtcur, TCPTV_REXMTMAX);   
        tcb->rxtcur = TCPTV_REXMTMAX; // the RTO
    }
    //LOG_INFO("[calc_rto] RTO updated to: %u ms", tcb->rxtcur);
}


/**
 * Handles the retransmission timer when it expires.
 * 
 * @param *tcb The TCB containing the expired timer.
 */
static void handle_rexmt_timeout(tcb_t *tcb)
{
    tcb->rxtshift++;

    /* Exponential backoff (RFC 6298, rule 5.5) */
    int backoff_mult = tcp_backoff[tcb->rxtshift];

    int base_rto = tcb->rxtcur > 0 ? tcb->rxtcur : TCPTV_SRTTDFLT;
    int new_timer = base_rto * backoff_mult;

    tcb->t_timer[TCPT_REXMT] = MIN(new_timer, TCPTV_REXMTMAX);

    uint32_t flight_size = tcb->snd_nxt - tcb->snd_una;
    uint32_t pre_rollback_snd_nxt = tcb->snd_nxt;
    tcb->snd_nxt = tcb->snd_una; // rollback the sequence number

    LOG_DEBUG("[handle_rexmt_timeout] Timeout #%d fired! "
                "base_rto=%d ticks (%d ms) x backoff=%d -> new_timer=%d ticks (%d ms). "
                "flight_size=%u bytes. snd_nxt rolled back: %u -> %u (snd_una).",
                tcb->rxtshift, base_rto, base_rto * TCP_TICK_MS, backoff_mult, new_timer,
                new_timer * TCP_TICK_MS, flight_size, pre_rollback_snd_nxt, tcb->snd_nxt);


    if (tcb->cc && tcb->cc->timeout)
    { // delegate to congestion control algo
        tcb->cc->timeout(tcb, flight_size);
    }

    send_dgram(tcb);
}


void utcp_timeout(tcb_t *tcb, short timer)
{
    switch(timer)
    {
        case TCPT_REXMT: // retransmit
            handle_rexmt_timeout(tcb);
            break;

        case TCPT_PERSIST: // persist
        /* 
         * There is data to send, but it is being stopped b/c the
         * receiver's advertised window (tcb->rwnd) is 0.
        */
        // TODO: tcp_setpersist calculates the next value for the persist
        // timer and stores it in the TCPT_PERSIST counter. The flag t_force
        // is set to 1, forcing tcp_output to send 1 byte, even though the window
        //advertised by the other end is 0.
            break;

        case TCPT_KEEP: // keepalive
            break;

        case TCPT_2MSL: // 2MSL, or FIN_WAIT_2
            break;

        case TCPT_DELACK: // delayed ACK
            break;
            
        default:
            err_sys("Invalid timer");
    }
}

int reset_timer(tcb_t *tcb, uint8_t timer_idx)
{
    if (timer_idx == TCPT_REXMT) 
    {
        int rearm_ticks = tcb->rxtcur;
        int backoff = 1;
        if (tcb->rxtshift > 0)
        {
            backoff = tcp_backoff[tcb->rxtshift];
            rearm_ticks = tcb->rxtcur * backoff;
            if (rearm_ticks > TCPTV_REXMTMAX)
                rearm_ticks = TCPTV_REXMTMAX;
        }
        tcb->t_timer[TCPT_REXMT] = rearm_ticks;
        LOG_DEBUG("[reset_timer] REXMT RESET: base_rto=%u, shift=%u, rxtcur=%u ms", 
                  tcb->rxtcur, tcb->rxtshift, tcb->rxtcur);
                  
        return tcb->rxtcur;
    }
    
    // Fallback for other timers
    tcb->t_timer[timer_idx] = tcb->rxtcur; 
    return tcb->rxtcur;
}

void pause_timer(tcb_t *tcb, uint8_t timer_idx)
{
    tcb->t_timer[timer_idx] = 0;
}