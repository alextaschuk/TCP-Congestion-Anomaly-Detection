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
    ts.tv_nsec = 1000000L; // 1 million nanoseconds (1ms)
    while (1) 
    {
        nanosleep(&ts, NULL); // sleep for 1ms
        utcp_slowtimo(); // wake up and process all TCP timers
    }
    
    return NULL;
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

    /* Compute the RTO */
    uint32_t current_srtt = tcb->srtt >> 3;
    uint32_t four_rttvar = tcb->rttvar;
    //LOG_INFO("[calc_rto] Calculating the RTO. The current srtt = %u, rttvar = %u", current_srtt, four_rttvar);

    tcb->rxtcur = current_srtt + MAX(CLOCK_GRANULARITY, four_rttvar);
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
        LOG_DEBUG("[calc_rto] RTO %u ticks clamped up to TCPTV_MIN=%d ticks.", tcb->rxtcur, TCPTV_REXMTMAX);   
        tcb->rxtcur = TCPTV_REXMTMAX; // the RTO
    }
    //TCPT_RANGESET(tcb->rxtcur, tcb->rxtcur, 200, 64000);
    //LOG_INFO("[calc_rto] RTO updated to: %u ms", tcb->rxtcur);
}


static void handle_rexmt_timeout(tcb_t *tcb)
{
    LOG_INFO("[handle_rexmt_timeout] REXMT timer expired for TCB %u", tcb->fd);
    tcb->rxtshift++;

    /* Exponential backoff (RFC 6298, rule 5.5) */
    //tcb->rxtcur = tcb->rxtcur * 2; // wait twice as long before trying again
    int backoff_mult = tcp_backoff[tcb->rxtshift];

    int base_rto = tcb->rxtcur > 0 ? tcb->rxtcur : TCPTV_SRTTDFLT;
    int new_timer = base_rto * backoff_mult;

    //TCPT_RANGESET(tcb->rxtcur, tcb->rxtcur, 200, 64000);
    if (new_timer > TCPTV_REXMTMAX)
        new_timer = TCPTV_REXMTMAX;
    tcb->t_timer[TCPT_REXMT] = new_timer;

    uint32_t flight_size = tcb->snd_nxt - tcb->snd_una;
    tcb->snd_nxt = tcb->snd_una; // rollback the sequence number

    if (tcb->cc && tcb->cc->timeout)
    { // delegate to congestion control algo
        tcb->cc->timeout(tcb, flight_size);
    }
    

    LOG_INFO("[handle_rexmt_timeout] cwnd dropped to %u, ssthresh set to %u",  tcb->cwnd, tcb->ssthresh);
    
    //const char *old_category = current_thread_cat;
    //current_thread_cat = "cc_data";
    //LOG_INFO("TIMEOUT,%u,%u", tcb->cwnd, tcb->ssthresh);
    //current_thread_cat = old_category;

    //tcb->dupacks = 0;
    //tcb->fast_recovery = false;

    send_dgram(tcb);

    //reset_timer(tcb, TCPT_REXMT);
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
         * receiver's advertised window (tcb->rcv_wnd) is 0.
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
        tcb->rxtshift = 0; // reset backoff shift back to 0 for Karn's algo

        // Calculate the backed-off RTO: base RTO shifted by the number of timeouts (rto * 2^rxtshift)
        //uint32_t backed_off_rto = tcb->rxtcur << tcb->rxtshift;
        
        // Enforce the bounds on the newly calculated timeout (e.g., max 64 seconds)
        //TCPT_RANGESET(backed_off_rto, backed_off_rto, 200, 64000);
        
        //tcb->rxtcur = backed_off_rto;
        tcb->t_timer[timer_idx] = tcb->rxtcur;
        
        LOG_DEBUG("[reset_timer] REXMT timer reset. base_rto=%u, shift=%u, rxtcur=%u ms", 
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