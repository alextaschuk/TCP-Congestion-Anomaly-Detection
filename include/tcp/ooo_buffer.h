#ifndef OOO_BUFFER_H
#define OOO_BUFFER_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

struct tcb_t;
typedef struct tcb_t tcb_t;

// Structure to hold an out-of-order segment
typedef struct ooo_segment {
    uint32_t seq;
    uint32_t len;
    uint8_t *data;
    struct ooo_segment *next;
} ooo_segment_t;


/**
 * @brief Insert an out-of-order segment into the TCB's reassembly queue.
 * 
 * The queue is kept sorted in ascending sequence-number order. Overlapping
 * and duplicate bytes are trimmed so the queue never holds redundant data.
 * `ooo_bytes` counts against the effective receive window to prevent the sender
 * from overrunning the buffer.
 *
 * @note Called with TCB lock held.
 */
void insert_ooo_segment(tcb_t *tcb, uint32_t, uint8_t *, uint32_t);

/**
 * @brief Drain consecutive entries from the out-of-order queue into the RX buffer.
 *
 * Called after an in-order segment is accepted. If the head of the OOO queue
 * now begins exactly `rcv_nxt`, that data is contiguous and can be moved into
 * `rx_buf`.  We repeat until the queue is empty or a gap remains, advancing
 * `rcv_nxt` cumulatively (i.e. a single cumulative ACK covers all drained data).
 *
 * @note Called with TCB lock held.
 */
void drain_ooo_queue(tcb_t *tcb);

#endif
