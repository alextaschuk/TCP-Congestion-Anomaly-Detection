#include <tcp/tcb_queue.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <tcp/tcb.h>
#include <utils/logger.h>
#include <utcp/api/globals.h>

int enqueue_tcb(tcb_t *tcb, tcb_queue_t *q)
{
    if (q->count >= q->backlog)
    {
        LOG_INFO("[enqueue_tcb] Queue is full, connection being dropped");
        return -1;
    }

    q->tcbs[q->tail] = tcb;
    q->tail = (q->tail + 1) % q->backlog;
    q->count++;

    return 0;
}


tcb_t* dequeue_tcb(tcb_queue_t *q)
{
    if (q->count == 0)
    {
        LOG_INFO("[dequeue_tcb] Queue is empty");
        return NULL;
    }

    tcb_t *tcb = q->tcbs[q->head];
    q->head = (q->head + 1) % MAX_BACKLOG;
    q->count--;

    return tcb;
}


tcb_t* remove_from_syn_queue(tcb_queue_t *q, uint32_t remote_ip, uint16_t remote_utcp_port)
{
    for (int i = 0; i < q->count; i++)
    {
        int idx = (q->head + i) % MAX_BACKLOG;
        tcb_t *tcb = q->tcbs[idx];
        
        // check if this TCB matches the incoming segment's source
        uint32_t dest_ip = tcb->fourtuple.dest_ip;
        uint16_t dest_udp_port = tcb->dest_udp_port;

        if (dest_ip == remote_ip && dest_udp_port == remote_utcp_port)
        { 
            for (int j = i; j < q->count - 1; j++)
            {
                int curr = (q->head + j) % MAX_BACKLOG;
                int next = (q->head + j + 1) % MAX_BACKLOG;
                q->tcbs[curr] = q->tcbs[next];
            }
            
            q->tail = (q->tail - 1 + MAX_BACKLOG) % MAX_BACKLOG;
            q->count--;
            
            return tcb;
        }
    }
    LOG_WARN("[remove_from_syn_queue] TCB not found in the SYN queue.");
    return NULL;
}
