/*
Logic related to application-level sending, receiving, managing,
and handling packets of data.
*/
#include <utcp/api/data.h>

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include <utcp/api/api.h>
#include <utcp/api/globals.h>
#include <utcp/api/tx_dgram.h>

#include <tcp/tcp_segment.h>

#include <utils/err.h>
#include <utils/printable.h>


ssize_t utcp_send(tcb_t *snder_tcb, int snder_sock, const void *buf, size_t payload_len)
{

    size_t free = ring_buf_free(&snder_tcb->tx_buf);
    if (free == 0)
        return 0;  // buffer full

    if (payload_len > free)
        payload_len = free; // write the max amount of data possible

    ring_buf_write(&snder_tcb->tx_buf, buf, payload_len);

    tcp_output(snder_tcb, snder_sock);
    
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

    tcb->rcv_wnd = ring_buf_free(&tcb->rx_buf);

    return len;
}


static void tcp_output(tcb_t *snder_tcb, int snder_sock) 
{
    uint32_t unacked_bytes_in_flight = snder_tcb->snd_nxt - snder_tcb->snd_una; // bytes that have been sent (in flight) (on the wire)
    size_t buffered_bytes = ring_buf_used(&snder_tcb->tx_buf); // data in tx_buf waiting to be sent
    
    // look for unsent data in the TX buffer
    if (buffered_bytes > unacked_bytes_in_flight)
    { // true = new data is in the buffer that has not be sent out
        size_t unsent_bytes = buffered_bytes - unacked_bytes_in_flight;
        size_t available_wnd = snder_tcb->snd_wnd;

        uint32_t to_send = MIN(unsent_bytes, available_wnd); // don't exceed amount receiver can store
        size_t len = MIN(MSS, to_send); // don't send more than the receiver can handle

        uint8_t tmp[MSS];
        
        // peek at offset 'flight' to skip the un-acked data we already sent
        ring_buf_peek_offset(&snder_tcb->tx_buf, tmp, len, unacked_bytes_in_flight);

        if (len > 0)
        {
            uint8_t tmp[MSS];
            
            // skip the in-flight bytes that we already sent but haven't rcv'd ACKs for yet
            ring_buf_peek_offset(&snder_tcb->tx_buf, tmp, len, unacked_bytes_in_flight);

            send_dgram(snder_tcb, snder_sock, tmp, len, 0);
        }
    }
}

