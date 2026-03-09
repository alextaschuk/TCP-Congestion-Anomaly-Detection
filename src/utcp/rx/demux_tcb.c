#include <utcp/rx/demux_tcb.h>

#include <stdio.h>

#include <tcp/hndshk_fsm.h>
#include <utils/logger.h>
#include <utcp/api/globals.h>

tcb_t* demux_tcb(
    api_t *global,
    uint16_t dest_utcp_port,
    uint32_t src_ip,
    uint16_t src_udp_port
)
{
    tcb_t *listen_tcb = NULL;

    pthread_mutex_lock(&global->lookup_lock);
    /* look for a matching TCB that has an ESTABLISHED state */
    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        tcb_t *tcb = global->tcb_lookup[i];

        if (tcb == NULL)
            continue;

        uint16_t local_utcp_port = tcb->fourtuple.source_port;
        uint32_t remote_ip = tcb->fourtuple.dest_ip;
        uint16_t remote_udp_port = tcb->dest_udp_port;

        // check 4-tuple 
        if (local_utcp_port == dest_utcp_port && remote_ip == src_ip && remote_udp_port == src_udp_port)
        {
            pthread_mutex_unlock(&global->lookup_lock);
            return tcb;
        }

        // we will return listener TCB to server if state isn't ESTABLISHED or SYN-RECEIVED
        if (local_utcp_port == dest_utcp_port && tcb->fsm_state == LISTEN)
        {
            //LOG_INFO("[demux_tcb] found listen TCB\n");
            listen_tcb = tcb;
            continue;
        }
    }

    // no active connection found, look for half-open connection (3WHS is in-progress)
    if (listen_tcb != NULL)
    {
        //LOG_INFO("[demux_tcb] Searching for TCB in SYN queue");

        pthread_mutex_lock(&listen_tcb->syn_q.lock);
        for (int i = 0; i < listen_tcb->syn_q.count; i++)
        {
            int idx = (listen_tcb->syn_q.head + i) % MAX_BACKLOG;
            tcb_t *syn_tcb = &listen_tcb->syn_q.tcbs[idx];

            if (syn_tcb == NULL)
                continue;
            
            uint32_t client_ip = syn_tcb->fourtuple.dest_ip;
            uint16_t client_udp_port = syn_tcb->dest_udp_port;

            if (client_ip == src_ip && client_udp_port == src_udp_port)
            {
                //LOG_INFO("[demux_tcb] Found a TCB with a half-open connection");
                pthread_mutex_unlock(&global->lookup_lock);
                pthread_mutex_unlock(&listen_tcb->syn_q.lock);
                return syn_tcb;
            }
        }

        pthread_mutex_unlock(&global->lookup_lock);
        pthread_mutex_unlock(&listen_tcb->syn_q.lock);
        
        //LOG_INFO("[demux_tcb] No TCB found; will handle incoming connection (SYN) request...");
        return listen_tcb;
    }

    pthread_mutex_unlock(&global->lookup_lock);
    LOG_WARN("[demux_tcb] No TCB found, nor listen socket found.");
    return NULL; 
}
