#ifndef TCB_QUEUE_H
#define TCB_QUEUE_H

#include <stdint.h>

#include <pthread.h>


typedef struct tcb_t tcb_t;

/**
 * @brief Contains the variables necessary to maintain
 * SYN and accept queues.
 * 
 * @param **tcbs The queue that stores `tcb_t` structs
 * @param backlog The max number of TCBs that can be stored at a given time
 * @param head Index of the queue's head
 * @param tail Index of the queue's tail
 * @param count Number of TCBs currently stored in the queue
 * @param lock Used to prevent race conditions
 * @param cond Used by `accept()` to block until a TCB is ready
 */
typedef struct tcb_queue_t
{
    tcb_t **tcbs;
    int backlog;
    int head;
    int tail;
    int count; // number of TCBs in the queue.
    pthread_mutex_t lock;
    pthread_cond_t cond;
} tcb_queue_t;

/*Begin define functions*/

/**
 * @brief Adds a `tcb_t` to the tail of a SYN or accept queue.
 * 
 * @param *tcb The TCB struct to add to the queue
 * @param *q The queue that `*tcb` is being added to.
 * 
 * @return `-1` if queue is full, or `0` on success.
 */
int enqueue_tcb(tcb_t *tcb, tcb_queue_t *q);

/**
 * @brief pops the `tcb_t` located at the head of `*q`
 * 
 * @return `NULL` if the queue is empty, or `tcb_t *tcb` on success.
 */
tcb_t* dequeue_tcb(tcb_queue_t *q);

/**
 * @brief removes a TCB from a SYN queue using a remote IP & UTCP port
 * 
 * @param *q The SYN queue.
 * @param remote_ip An IPv4 address.
 * @param remote_port A UTCP port.
 */
tcb_t* remove_from_syn_queue(tcb_queue_t *q, uint32_t remote_ip, uint16_t remote_utcp_port);

/*End define functions*/

#endif
