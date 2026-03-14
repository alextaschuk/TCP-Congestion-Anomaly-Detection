#include <tcp/ooo_buffer.h>

#include <stdlib.h>
#include <string.h>

#include <tcp/tcb.h>
#include <utcp/api/api.h>
#include <utils/logger.h>


static void insert_ooo_segment(tcb_t *tcb, uint32_t seq, uint8_t *data, uint32_t len)
{
    /* Available space: what isn't already used by in-order or OOO bytes */
    uint32_t buf_used = tcb->rx_tail - tcb->rx_head;
    uint32_t available = BUF_SIZE - buf_used - tcb->ooo_bytes;

    if (len > available)
    {
        LOG_WARN("[insert_ooo_segment] OOO DROP: No buffer space. buf_used=%u ooo_bytes=%u incoming=%u",
                    buf_used, tcb->ooo_bytes, len);
        return;
    }

    uint32_t end_seq = seq + len; /* exclusive end of incoming segment */

    /* Walk to the insertion point: skip entries that end before our segment starts */
    ooo_segment_t *prev = NULL;
    ooo_segment_t *cur = tcb->ooo_head;

    while (cur != NULL && SEQ_LEQ(cur->seq + cur->len, seq))
    {
        prev = cur;
        cur = cur->next;
    }

    /* Trim start: the tail of the previous entry may overlap our new segment */
    if (prev != NULL)
    {
        uint32_t prev_end = prev->seq + prev->len;
        if (SEQ_GT(prev_end, seq))
        {
            uint32_t overlap = prev_end - seq;
            if (overlap >= len)
            {
                LOG_DEBUG("[insert_ooo_segment]: seg [%u,%u) fully covered by existing entry. Discarding.", seq, end_seq);
                return;
            }
            data += overlap;
            len -= overlap;
            seq = prev_end;
            end_seq = seq + len;
        }
    }

    ooo_segment_t *entry = malloc(sizeof(ooo_segment_t));
    if (!entry)
    {
        LOG_ERROR("[insert_ooo_segment] malloc failed for tcpq_entry");
        return;
    }

    entry->data = malloc(len);
    if (!entry->data)
    {
        free(entry);
        LOG_ERROR("[insert_ooo_segment]: malloc failed for OOO data buffer");
        return;
    }

    memcpy(entry->data, data, len);
    entry->seq = seq;
    entry->len = len;
    entry->next = NULL;

    /* Absorb or trim any following entries that our new segment overlaps */
    while (cur != NULL && SEQ_LT(cur->seq, end_seq))
    {
        uint32_t cur_end = cur->seq + cur->len;
        
        if (SEQ_LEQ(cur_end, end_seq))
        {
            /* cur is fully covered — remove it */
            struct tcpq_entry *next = cur->next;
            tcb->ooo_bytes -= cur->len;
            free(cur->data);
            free(cur);
            cur = next;
        }
        else
        {
            /* cur partially overlaps — trim its front */
            uint32_t overlap = end_seq - cur->seq;
            memmove(cur->data, cur->data + overlap, cur->len - overlap);
            tcb->ooo_bytes -= overlap;
            cur->seq += overlap;
            cur->len -= overlap;
            break;
        }
    }

    /* Link the new entry into the sorted list */
    entry->next = cur;
    if (prev == NULL)
    {
        tcb->ooo_head = entry;
    }
    else
    {
        prev->next = entry;
    }

    tcb->ooo_bytes += len;
    LOG_INFO("OOO BUFFERED: seg [%u, %u). ooo_bytes now %u", seq, end_seq, tcb->ooo_bytes);
}


void drain_ooo_queue(tcb_t *tcb)
{
    while (tcb->ooo_head != NULL)
    {
        ooo_segment_t *entry = tcb->ooo_head;
        uint32_t entry_end = entry->seq + entry->len;

        /* Case 1: Fully redundant — the entire entry falls below rcv_nxt.
         * This can happen when a large in-order segment overlaps data that
         * was already sitting in the OOO queue. Discard and keep scanning;
         * the next entry may still be useful.
         */
        if (SEQ_LEQ(entry_end, tcb->rcv_nxt))
        {
            LOG_WARN("[drain_ooo_queue]: Discarding fully redundant entry [%u, %u) (rcv_nxt=%u).",
                        entry->seq, entry_end, tcb->rcv_nxt);
            tcb->ooo_bytes -= entry->len;
            tcb->ooo_head = entry->next;
            free(entry->data);
            free(entry);
            continue;
        }

        /* Case 2: Partial overlap — the entry starts before rcv_nxt but
         * extends past it.  Trim the already-received prefix in place so
         * the entry aligns exactly with rcv_nxt, then fall through.
         */
        if (SEQ_LT(entry->seq, tcb->rcv_nxt))
        {
            uint32_t trim = tcb->rcv_nxt - entry->seq;
            LOG_WARN("[drain_ooo_queue]: Trimming %u redundant bytes from entry [%u, %u) (rcv_nxt=%u).", trim, entry->seq,
                       entry_end, tcb->rcv_nxt);
            memmove(entry->data, entry->data + trim, entry->len - trim);
            tcb->ooo_bytes -= trim;
            entry->seq += trim;
            entry->len -= trim;
            /* entry->seq == rcv_nxt after trim; fall through to Case 3 */
        }

        /* Case 3: Entry starts exactly at rcv_nxt — drain it into recv_buf */
        if (entry->seq != tcb->rcv_nxt) {
            break; /* Gap still exists; nothing more to drain */
        }

        uint32_t free_space = BUF_SIZE - (tcb->rx_tail - tcb->rx_head);
        if (entry->len > free_space) {
            LOG_ERROR("[drain_ooo_queue]: recv_buf full during drain! entry->len=%u free=%u", entry->len, free_space);
            break;
        }

        ring_buf_write(tcb->rx_buf, BUF_SIZE, tcb->rx_tail, entry->data, entry->len);
        tcb->rx_tail += entry->len;
        tcb->rcv_nxt += entry->len;
        tcb->ooo_bytes -= entry->len;

        tcb->ooo_head = entry->next;
        free(entry->data);
        free(entry);

        dzlog_info("[drain_ooo_queue]: entry consumed, rcv_nxt=%u ooo_bytes=%u", tcb->rcv_nxt, tcb->ooo_bytes);
    }
}