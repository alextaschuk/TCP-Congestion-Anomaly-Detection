#ifndef DATA_H
#define DATA_H

#include <stdint.h>

#include <netinet/in.h>
#include <sys/types.h>

#include <tcp/tcb.h>
#include <utcp/api/globals.h>


/**
 * @brief An application-side function used to send data.
 * 
 * Equivalent to the `send()` syscall. This function writes `payload_len`
 * number of bytes, or the maximum amount available, to the `tx_buffer`,
 * then calls `tcp_output()` to send the packet. If there is no room in
 * TX, the application thread blocks until space is made available.
 * 
 * @param utcp_fd An application's UTCP socket file descriptor to send data
 * out of.
 * @param *buf The payload of data to send.
 * @param payload_len The length of the payload, in bytes.
 * 
 * @return The number of bytes sent (i.e., written to `tx_buf`)
 */
ssize_t utcp_send(int utcp_fd, const void *buf, size_t payload_len);

/**
 * @brief An application-side function used to receive data. This is how an app reads from `rx_buf`.
 * 
 * Equivalent to the `recv()` syscall.
 * 
 * @param utcp_fd A application's UTCP file descriptor to receive data in to.
 * @param *buf The application's buffer to receive (read) the data into.
 * @param app_buf_len The max amount of data the application can store.
 * 
 * @return The number of bytes sent (i.e., read from `rx_buf`).
 */
ssize_t utcp_recv(int utcp_fd, uint8_t *buf, size_t app_buf_len);

#endif
