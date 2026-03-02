#include <utcp/api/utcp_timers.h>

#include <netinet/tcp_timer.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#include <utcp/api/globals.h>
#include <utils/err.h>


void* utcp_ticker_thread(void) 
{
    pthread_mutex_t lookup_lock = PTHREAD_MUTEX_INITIALIZER; /* prevents iterating while a new connection is being added/removed */
    printf("[utcp_ticker_thread] UTCP 500ms slow timer started.\n");

    struct timespec ts;
    ts.tv_sec = 0; // 0 seconds
    ts.tv_nsec = 500000000L; // 500 million nanoseconds = 500ms

    while (1) 
    {
        nanosleep(&ts, NULL); // sleep for 500ms
        utcp_slowtimo(lookup_lock); // wake up and process all TCP timers
    }
    
    return NULL;
}


void utcp_slowtimo(pthread_mutex_t lookup_lock) 
{
    pthread_mutex_lock(&lookup_lock);

    api_t *global = api_instance();

    for (int i = 0; i < MAX_CONNECTIONS; i++) 
    {
        tcb_t *tcb = global->tcb_lookup[i];
        if (!tcb)
            continue; 

        for (int timer_idx = 0; timer_idx < TCPT_NTIMERS; timer_idx++) 
        {
            if (tcb->t_timer[timer_idx] > 0) 
            {
                tcb->t_timer[timer_idx]--;

                if (tcb->t_timer[timer_idx] == 0) 
                    utcp_timeout(tcb, timer_idx);
            }
        }
    }

    pthread_mutex_unlock(&lookup_lock);
}


void* utcp_timeout(tcb_t *tcb, short timer)
{
    switch(timer)
    {
        case TCPT_REXMT: // retransmit
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

uint32_t tcp_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint32_t)((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000));
}


void calc_rto(tcb_t *tcb)
{
    uint32_t current_time = tcp_now();
    uint32_t rtt_sample = current_time - tcb->ts_rcv_val; // This is R, or R'
    printf("[RTT] Raw sample: %u ms\n", rtt_sample);

    /* Calculate RTT with Jacobson/Karels Algorithm and set/update the RTO */
    if (tcb->srtt == 0)
    { // first RTT measurement R
        tcb->srtt = rtt_sample << 3;   // SRTT = RTT * 8
        tcb->rttvar = rtt_sample << 1; // RTTVAR = RTT / 2 * 4
    } else
    { // subsequent RTT measurement R'
        int32_t err = rtt_sample - (tcb->srtt >> 3); // `err` is ((R' - SRTT) / 8)

        tcb->srtt += err; // SRTT_new = SRTT_old + (err / 8)

        if (err < 0) err = -err; // compute |err|
        tcb->rttvar += err - (tcb->rttvar >> 2); // RTTVAR_new = RTTVAR_old + (|err| - RTTVAR_old) / 4
    }

    /* Compute the RTO */
    int32_t rto = tcb->srtt + MAX(CLOCK_GRANULARITY, tcb->rttvar);

    tcb->rto = (tcb->srtt >> 3) + tcb->rttvar;

    /**
     * Enforce bounds (e.g., Min 200ms, Max 60 seconds)
     * @note Strict RFC 6298 says min should be 1000ms, but Linux uses 200ms.
     */
    TCPT_RANGESET(tcb->rto, tcb->rto, 200, 60000);

    printf("[RTT] Updated RTO to: %u ms\n", tcb->rto);
}