#include <tcp/ooo_buffer.h>
#include <tcp/tcb.h>
#include <utils/logger.h>
#include <stdlib.h>
#include <string.h>

int ooo_buffer_add(tcb_t *tcb, uint32_t seq, uint32_t len, const uint8_t *data) {
    if (len == 0) {
        return 0;
    }

    ooo_segment_t *new_segment = (ooo_segment_t *)malloc(sizeof(ooo_segment_t));
    if (!new_segment) {
        LOG_ERROR("Failed to allocate memory for out-of-order segment");
        return -1;
    }

    new_segment->data = (uint8_t *)malloc(len);
    if (!new_segment->data) {
        LOG_ERROR("Failed to allocate memory for out-of-order segment data");
        free(new_segment);
        return -1;
    }

    new_segment->seq = seq;
    new_segment->len = len;
    memcpy(new_segment->data, data, len);
    new_segment->next = NULL;

    if (!tcb->ooo_head || SEQ_LT(seq, tcb->ooo_head->seq)) {
        new_segment->next = tcb->ooo_head;
        tcb->ooo_head = new_segment;
    } else {
        ooo_segment_t *current = tcb->ooo_head;
        while (current->next && SEQ_LT(current->next->seq, seq)) {
            current = current->next;
        }
        new_segment->next = current->next;
        current->next = new_segment;
    }

    LOG_INFO("Buffered out-of-order segment: seq=%u, len=%u", seq, len);
    return 0;
}

void ooo_buffer_process(tcb_t *tcb) {
    ooo_segment_t *current = tcb->ooo_head;
    ooo_segment_t *prev = NULL;

    while (current) {
        if (current->seq == tcb->rcv_nxt) {
            uint32_t rx_free_space = BUF_SIZE - (tcb->rx_tail - tcb->rx_head);
            if (current->len <= rx_free_space) {
                memcpy(&tcb->rx_buf[tcb->rx_tail % BUF_SIZE], current->data, current->len);
                tcb->rx_tail += current->len;
                tcb->rcv_nxt += current->len;

                LOG_INFO("Processed buffered segment: seq=%u, len=%u", current->seq, current->len);

                if (prev) {
                    prev->next = current->next;
                } else {
                    tcb->ooo_head = current->next;
                }
                
                ooo_segment_t *to_free = current;
                current = current->next;
                free(to_free->data);
                free(to_free);
            } else {
                LOG_ERROR("RX buffer is full. Cannot process buffered segment.");
                break;
            }
        } else {
            prev = current;
            current = current->next;
        }
    }
}
