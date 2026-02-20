#include <utcp/api/ring_buffer.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <utils/err.h>

#include <utcp/api/globals.h>

int ring_buf_init(ring_buf_t *c)
{
    c->buf = (uint8_t *)malloc(BUF_SIZE);
    if (!c->buf)
        err_sys("tcp_ringbuf_init");
    c->size = BUF_SIZE;
    c->head = 0;
    c->tail = 0;
    return 0;
}


size_t ring_buf_used(const ring_buf_t *c)
{
    int used = c->head - c->tail;
    if (used < 0)
        used += 2 * c->size;

    if (used > c->size)
        used -= c->size;

    return (size_t)used;
}


size_t ring_buf_free(const ring_buf_t *c)
{
    return c->size - ring_buf_used(c);
}


size_t ring_buf_read(ring_buf_t *c, uint8_t *dest, size_t len)
{
    printf("[ring_buf_read] Reading from the ring buffer\n");
    size_t used = ring_buf_used(c);
    if (len > used)
        len = used;

    size_t first_chunk = c->size - (c->tail % c->size);
    if (first_chunk > len)
        first_chunk = len;

    if (dest != NULL)
    {
        memcpy(dest, c->buf + (c->tail % c->size), first_chunk);
        memcpy(dest + first_chunk, c->buf, len - first_chunk);
    }

    c->tail = (c->tail + len) % (2 * c->size);
    return len;
}


size_t ring_buf_write(ring_buf_t *c, const uint8_t *data, size_t len)
{
    size_t free_space = ring_buf_free(c);
    if (len > free_space)
        len = free_space;

    size_t first_chunk = c->size - (c->head % c->size);
    if (first_chunk > len)
        first_chunk = len;

    memcpy(c->buf + (c->head % c->size), data, first_chunk);
    memcpy(c->buf, data + first_chunk, len - first_chunk);

    c->head = (c->head + len) % (2 * c->size);  // allow 2*size wraparound
    return len;
}


size_t ring_buf_peek(const ring_buf_t *c, uint8_t *dest, size_t len)
{
    size_t used = ring_buf_used(c);
    if (len > used)
        len = used;

    size_t first_chunk = c->size - (c->tail % c->size);
    if (first_chunk > len)
        first_chunk = len;

    memcpy(dest, c->buf + (c->tail % c->size), first_chunk);
    memcpy(dest + first_chunk, c->buf, len - first_chunk);

    return len;  // tail not advanced
}


void ring_buf_wipe(ring_buf_t *c)
{
    free(c->buf);
    c->buf = NULL;
    c->size = 0;
    c->head = c->tail = 0;
}

