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
    pthread_mutex_lock(&snder_tcb->lock); // lock the TCB

    size_t free = ring_buf_free(&snder_tcb->tx_buf);

    while (free == 0)
    {
    if (snder_tcb->fsm_state != ESTABLISHED && snder_tcb->fsm_state != CLOSE_WAIT)
    { // network's connection dropped; unlock and exit.
        pthread_mutex_unlock(&snder_tcb->lock);
        return -1;
    }

    printf("[utcp_send] TX buffer is full. App thread going to sleep...\n");
    pthread_cond_wait(&snder_tcb->conn_cond, &snder_tcb->lock);

    free = ring_buf_free(&snder_tcb->tx_buf);
    }

    if (payload_len > free)
        payload_len = free; // write the max amount of data possible

    ring_buf_write(&snder_tcb->tx_buf, buf, payload_len);

    tcp_output(snder_tcb, snder_sock);

    pthread_mutex_unlock(&snder_tcb->lock);
    
    return (ssize_t)payload_len;
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
    
    /**
     * The effective window is the max amount of unACKed data that can be in-flight at
     * once. It is limited by either the receiver's capacity or the network's capacity.
     */
    uint32_t effective_wnd = MIN(snder_tcb->snd_wnd, snder_tcb->snd_cwnd);

    /* 2. Calculate the Usable Window */
    if (unacked_bytes_in_flight >= effective_wnd) {
        return; // there's no room to send more data.
    }
    uint32_t usable_wnd = effective_wnd - unacked_bytes_in_flight; // how many bytes can be sent

    // look for unsent data in the TX buffer
    if (buffered_bytes > unacked_bytes_in_flight)
    { // true = new data is in the buffer that has not be sent out
        size_t unsent_bytes = buffered_bytes - unacked_bytes_in_flight;
        
        // We can only send what we have (bounded by the usable window)
        uint32_t bytes_to_send = MIN((uint32_t)unsent_bytes, usable_wnd); 

        // Slice the data into MSS-sized chunks and send them out
        while (bytes_to_send > 0)
        {
            uint32_t len = MIN((uint32_t)MSS, bytes_to_send); 
            uint8_t tmp[MSS];
            
            // Peek at the offset to skip over the data that is already in flight
            ring_buf_peek_offset(&snder_tcb->tx_buf, tmp, len, unacked_bytes_in_flight);

            send_dgram(snder_tcb, snder_sock, tmp, len, TH_ACK | TH_PUSH);

            bytes_to_send -= len;
            unacked_bytes_in_flight += len;
        }
    }
}

