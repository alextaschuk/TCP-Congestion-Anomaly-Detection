#ifndef DATA_H
#define DATA_H

#include <netinet/in.h>
#include <stdint.h>
#include <sys/types.h>

#include <tcp/tcb.h>

uint8_t* build_datagram(uint16_t dest_port, char *dest_ip, uint8_t *buffer); // TODO

/**
 * @brief send a buffer of data to a socket.
 * 
 * @param sock UDP sock fd to send the datagram out of
 * @param *tcb The sender's TCB
 * @param *buf data payload to append after the TCP header
 * @param len number of bytes in data payload
 * @param flags TCP header flags to append to the TCP header
 * 
 * @return `bytes_sent` the number of bytes sent in the
 * datagram, or `-1` if fails
 */
int send_dgram(int sock, tcb_t *tcb, void *buf, size_t len, int flags);

/**
 * @brief receive a datagram and update TCB values as needed.
 * 
 * @note For a UDP socket to receive a datagram, it needs to call this function. 
 */
ssize_t rcv_dgram(int sock, tcb_t *tcb, ssize_t buflen);

/**
 * @brief this function is called by an application to write
 * to the transmit (send) buffer (`tx_buf`).
 */
ssize_t utcp_send(int utcp_fd, const void *buf, size_t len);

/**
 * @brief this function is called by an application
 * to read from the receive buffer (`rx_buf`).
 */
ssize_t utcp_recv(int utcp_fd, void *buf, size_t len);

#endif
