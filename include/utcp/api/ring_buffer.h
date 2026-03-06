#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stddef.h>
#include <stdint.h>

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
    uint8_t *buf;   /* the ring buffer */
    int size;       /* length of the buffer, in bytes */
    int head;
    int tail;       /* Points to oldest unread data */
} ring_buf_t;

/**
 * @brief Initializes a new circular buffer: either for a receive buffer (rx) or a send buffer (tx).
 * 
 * @param *c Pointer to a `ring_buf_t` struct stored in a TCB.
 */
int ring_buf_init(ring_buf_t *c);

/**
 * Tells us how many bytes are currently used in a ring buffer
 * 
 * @param *c Pointer to a ring buffer.
 * 
 * @return The amount of space currently unavailable in `*c`, in bytes.
 */
size_t ring_buf_used(const ring_buf_t *c);

/**
 * Tells us how many bytes are free in a ring buffer.
 * 
 * @param *c Pointer to a ring buffer.
 * 
 * @return The amount of free space currently available in `*c`, in bytes.
 */
size_t ring_buf_free(const ring_buf_t *c);

/**
 * Read a stream bytes from a buffer.
 * 
 * @param *c Pointer to a ring buffer.
 * @param *dest A data buffer that the read stream of bytes is stored in.
 * @param len The number of bytes to read from `*c`.
 * 
 * @note This is a destructive function. The bytes that are read are consumed.
 */
size_t ring_buf_read(ring_buf_t *c, uint8_t *dest, size_t len);

/**
 * @brief Write a stream of bytes into a buffer.
 * 
 * This function will write up to `ring_buf_free(c)` bytes into the buffer.
 * 
 * @param *c Pointer to ring buffer.
 * @param *data A stream of bytes to write into the buffer.
 * @param len The number of bytes to write.
 * 
 * @return `size_t` number of bytes written
 */
size_t ring_buf_write(ring_buf_t *c, const uint8_t *data, size_t len);

/**
 * Peek a stream of bytes from buffer without consuming data.
 * 
 * @param *c Pointer to ring buffer.
 * @param *dest A data buffer that the peeked stream of bytes is stored in.
 * @param len The number of bytes to read from `*c`.
 * 
 * @note This is the non-destructive version of `ring_buf_read()`.
 */
size_t ring_buf_peek(const ring_buf_t *c, uint8_t *dest, size_t len);


/**
 * Peek a stream of bytes from buffer without consuming data, starting at a given offset.
 * 
 * @param *c Pointer to a ring buffer.
 * @param *dest A data buffer that the peeked stream of bytes is stored in.
 * @param len The number of bytes to read from `*c`.
 * @param offest The offset.
 * 
 * @note This is helpful for situations where we need to retransmit data. E.g., we send packet(1),
 * get ACK(1), send packet(2), send packet(3), get ACK(3). We need to retransmit packet(2), and we 
 * can do so with this function.
 */
size_t ring_buf_peek_offset(const ring_buf_t *c, uint8_t *dest, size_t len, size_t offset);

#endif
