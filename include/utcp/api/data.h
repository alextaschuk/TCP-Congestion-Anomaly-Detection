#ifndef DATA_H
#define DATA_H

#include <netinet/in.h>
#include <stdint.h>
#include <sys/types.h>

#include <tcp/tcb.h>

/**
 * @brief An application would call this function to write
 * to the tx buffer.
 * 
 * @param *tcb pointer to the sender's TCB
 * @param *buf pointer to the payload of data to send
 * @param payload_len number of bytes that make up the payload
 */
ssize_t utcp_send(tcb_t *tcb, const void *buf, size_t payload_len);

/**
 * @brief this function is to be called by an application
 * for it read from the rx buffer.
 */
ssize_t utcp_recv(int utcp_fd, void *buf, size_t len);

#endif
