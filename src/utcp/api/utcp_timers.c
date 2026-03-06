#include <utcp/api/utcp_timers.h>

#include <netinet/tcp_timer.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#include <utcp/api/globals.h>
#include <utils/err.h>
#include <tcp/hndshk_fsm.h>
#include <utcp/api/tx_dgram.h>

// logger stuff
#include <utils/logger.h>

void* utcp_ticker_thread(void) 
{
    current_thread_cat = "ticker_thread";
    LOG_INFO("[utcp_ticker_thread] UTCP 500ms slow timer started...");

    struct timespec ts;
    ts.tv_sec = 0; // 0 seconds
    ts.tv_nsec = 500000000L; // 500 million nanoseconds = 500ms

    while (1) 
    {
        nanosleep(&ts, NULL); // sleep for 500ms
        utcp_slowtimo(); // wake up and process all TCP timers
    }
    
    return NULL;
}


void utcp_slowtimo(void) 
{
    api_t *global = api_instance();

    pthread_mutex_lock(&global->lookup_lock);

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
                LOG_INFO("[utcp_slowtimo] Updated timer %i. New value is %hu\n", timer_idx, tcb->t_timer[timer_idx]);

                if (tcb->t_timer[timer_idx] == 0)
                {
                    LOG_INFO("[utcp_slowtimo] Timer %i timed out.\n", timer_idx);
                    utcp_timeout(tcb, timer_idx);
                }
            }
        }
    }

    pthread_mutex_unlock(&global->lookup_lock);
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
    printf("[calc_rto] Raw sample: %u ms\n", rtt_sample);

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
    uint32_t current_srtt = tcb->srtt >> 3;
    uint32_t four_rttvar = tcb->rttvar;

    tcb->rto = current_srtt + MAX(CLOCK_GRANULARITY, four_rttvar);

    /**
     * Enforce bounds (e.g., Min 200ms, Max 60 seconds)
     * @note Strict RFC 6298 says min should be 1000ms, but Linux uses 200ms.
     */
    TCPT_RANGESET(tcb->rto, tcb->rto, 200, 60000);
    printf("[calc_rto] RTO value in TCB before update: %u\n", tcb->rto);
    printf("[calc_rto] Updated RTO to: %u ms\n", tcb->rto);
}


static void handle_rexmt_timeout(tcb_t *tcb)
{
    printf("[handle_rexmt_timeout] REXMT timer expired for TCB %u -> %u\n", tcb->fourtuple.source_port, tcb->fourtuple.dest_port);

    /* Exponential backoff */
    tcb->rto = tcb->rto * 2; // wait twice as long before trying again
    TCPT_RANGESET(tcb->rto, tcb->rto, 200, 60000);

    /* Set ssthresh to 50% of cwnd, reset cwnd, and re-enter slow start */
    tcb->snd_ssthresh = tcb->snd_cwnd >> 1;
    tcb->snd_cwnd = MSS;
    
    printf("[handle_rexmt_timeout] cwnd dropped to %u, ssthresh set to %u\n",  tcb->snd_cwnd, tcb->snd_ssthresh);

    retransmit_data(tcb);

    int ticks = (tcb->rto + 499) / 500; // restart the timer
    tcb->t_timer[TCPT_REXMT] = ticks;
}

int reset_timer(tcb_t *tcb, uint8_t timer_idx)
{
    int ticks = (tcb->rto + 499) / 500;
    tcb->t_timer[timer_idx] = ticks;
}

void pause_timer(tcb_t *tcb, uint8_t timer_idx)
{
    tcb->t_timer[timer_idx] = 0;
}