#ifndef DATA_H
#define DATA_H

#include <netinet/in.h>
#include <stdint.h>
#include <sys/types.h>

#include <tcp/tcb.h>

/**
 * @brief An application-side function used to send data.
 * 
 * This function writes `payload_len` number of bytes, or the maximum amount available,
 * to the `tx_buffer`, then calls `tcp_output()` to send the packet.
 * 
 * @param *snder_tcb pointer to the sending application's TCB
 * @param snder_sock The sending application's UDP socket FD.
 * @param *buf A pointer to the payload of data to send.
 * @param payload_len The length of the payload, in bytes.
 * 
 * @return The number of bytes sent (i.e., written to `tx_buf`)
 */
ssize_t utcp_send(tcb_t *snder_tcb, int snder_sock, const void *buf, size_t payload_len);

/**
 * @brief An application-side function used to receive data. This is how an app reads from `rx_buf`.
 * 
 * @param utcp_fd The receiver's UTCP file descriptor.
 * @param *buf A buffer to receive the data into.
 * @param len The amount of data that is being received, in bytes.
 * 
 * @return The number of bytes sent (i.e., read from `rx_buf`).
 */
ssize_t utcp_recv(int utcp_fd, void *buf, size_t len);


/**
 * @brief A helper function for `utcp_send()` to determine how many bytes
 * in a TX buffer can be sent.
 * 
 * This function checks how many unacked bytes are in flight and how many are
 * in `tx_buffer` in total. If there are more in the buffer than are in flight,
 * then some bytes are yet to be sent out; these bytes (up to the receiver's
 * `rcv_wnd` size - 1) are sent.
 * 
 * @param *snder_tcb The sending application's TCB
 * @param snder_sock The sending application's UDP socket FD.
 */
static void tcp_output(tcb_t *snder_tcb, int snder_sock);

#endif
