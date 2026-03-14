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

// Function to add a segment to the out-of-order buffer
int ooo_buffer_add(tcb_t *tcb, uint32_t seq, uint32_t len, const uint8_t *data);

// Function to process the out-of-order buffer
void ooo_buffer_process(tcb_t *tcb);

#endif // OOO_BUFFER_H
