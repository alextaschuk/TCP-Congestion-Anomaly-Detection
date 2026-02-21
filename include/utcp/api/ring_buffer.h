#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stddef.h>

/**
 * To ensure that data can be received and delivered in order the client and server
 * will each maintain a send (AKA transmit, or tx) a receive (rx) buffer. These will be implemented as circular 
 * (or ring) buffers.
 * 
 * - I followed this article to implement them: https://embedjournal.com/implementing-circular-buffer-embedded-c/
 * 
 * The rx buffer stores in-order payload bytes that have not yet been consumed by the application. The tx buffer
 * stores in-order bytes that an application has sent out but have not yet been ACKed by the receiver,
 * 
 * @note The amount of bytes that the buffers will be able to hold will be equal to some `BUF_SIZE macro + 1 byte`.
 * The buffer stores an extra byte because "there is no *clean way* to differentiate the buffer full vs empty cases."
 * A simple solution is to store an extra byte: 
 * 
 * - `if head == tail`, the buffer is empty
 * 
 * - `if (head + 1) == tail`, the buffer is full (drop new data)
 */
typedef struct ring_buf_t
{
    uint8_t *buf;
    int size;
    int head;
    int tail;
} ring_buf_t;

/**
 * @brief Initializes a new circular buffer; either for a receive buffer (rx) or a send buffer (tx).
 * 
 * @param *c Pointer to a ring_buf_t struct stored in a TCB struct.
 */
int ring_buf_init(ring_buf_t *c);

/**
 * @brief returns the number of bytes currently
 * stored in a given buffer.
 */
size_t ring_buf_used(const ring_buf_t *c);

/**
 * @param *c pointer to a ring buffer
 * 
 * @return `size_t` amount of free space (number
 * of bytes) currently available in a given
 * buffer.
 */
size_t ring_buf_free(const ring_buf_t *c);


size_t ring_buf_read(ring_buf_t *c, uint8_t *dest, size_t len); // Read bytes from buffer (consume them), returns number read

/**
 * @brief Write `len` bytes into the buffer.
 * 
 * @param *c pointer to ring buffer
 * @param *data stream of bytes to be written
 * @param len number of bytes in the stream
 * 
 * @return `size_t` number of bytes written
 */
size_t ring_buf_write(ring_buf_t *c, const uint8_t *data, size_t len);

/**
 * @brief Peek bytes from buffer without consuming data.
 */
size_t ring_buf_peek(const ring_buf_t *c, uint8_t *dest, size_t len);

void ring_buf_wipe(ring_buf_t *c); // frees allocated memory and resets pointers to null or 0

#endif
