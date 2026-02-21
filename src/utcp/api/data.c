/*
Logic related to sending, receiving, managing,
handling, etc. datagrams and the data inside of them
*/
#include <utcp/api/data.h>

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include <utcp/api/api.h>
#include <utcp/api/globals.h>

#include <tcp/tcp_segment.h>

#include <utils/err.h>
#include <utils/printable.h>


ssize_t utcp_send(tcb_t *tcb, const void *buf, size_t payload_len)
{

    size_t free = ring_buf_free(&tcb->tx_buf);
    if (free == 0)
        return 0;  // buffer full

    if (payload_len > free)
        payload_len = free; // write the max amount of data possible

    ring_buf_write(&tcb->tx_buf, buf, payload_len);
    
    return payload_len;
}


ssize_t utcp_recv(int utcp_fd, void *buf, size_t len)
{
    tcb_t *tcb = get_tcb(utcp_fd);

    size_t used = ring_buf_used(&tcb->rx_buf);
    if (used == 0)
        return 0; // no data available

    if (len > used)
        len = used; // read the max data amount of data possible

    ring_buf_read(&tcb->rx_buf, buf, len);

    // Update window after app consumes data
    printf("[utcp_recv] rcv_wnd before update: %u\n", tcb->rcv_wnd);
    tcb->rcv_wnd = ring_buf_free(&tcb->rx_buf);
    printf("[utcp_recv] rcv_wnd after update: %u\n", tcb->rcv_wnd);

    return len;
}
