/*
Logic related to application-level sending, receiving, managing,
and handling packets of data.
*/
#include <utcp/api/data.h>

#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include <tcp/tcp_segment.h>
#include <utils/err.h>
#include <utils/logger.h>
#include <utils/printable.h>
#include <utcp/api/api.h>
#include <utcp/api/globals.h>
#include <utcp/api/tx_dgram.h>


ssize_t utcp_send(tcb_t *snder_tcb, int snder_sock, const void *buf, size_t payload_len)
{
    pthread_mutex_lock(&snder_tcb->lock);

    size_t free = ring_buf_free(&snder_tcb->tx_buf);

    while (free == 0)
    { // block until there is room to send
    if (snder_tcb->fsm_state != ESTABLISHED && snder_tcb->fsm_state != CLOSE_WAIT)
    { // network's connection dropped; unlock and exit.
        pthread_mutex_unlock(&snder_tcb->lock);
        return -1;
    }

    LOG_DEBUG("[utcp_send] TX buffer is full for tcb %i. App thread going to sleep...", snder_tcb->fd);
    pthread_cond_wait(&snder_tcb->conn_cond, &snder_tcb->lock);

    free = ring_buf_free(&snder_tcb->tx_buf);
    }

    if (payload_len > free)
    {
        payload_len = free; // write the max amount of data possible
        //LOG_WARN("[utcp_send] Cannot fit all received data into the buffer. Truncating down to %zu bytes", payload_len);
    }

    ring_buf_write(&snder_tcb->tx_buf, buf, payload_len);
    tcp_output(snder_tcb, snder_sock);

    pthread_mutex_unlock(&snder_tcb->lock);
    
    return (ssize_t)payload_len;
}


ssize_t utcp_recv(int utcp_fd, void *buf, size_t app_buf_len)
{
    tcb_t *tcb = get_tcb(utcp_fd);
    if (!tcb)
    {
        LOG_ERROR("[utcp_recv] Invalid UTCP FD");
        return -1;
    }

    LOG_DEBUG("[utcp_recv] Locking the TCB for UTCP FD %i...", tcb->fd);
    pthread_mutex_lock(&tcb->lock);

    while (ring_buf_used(&tcb->rx_buf) == 0)
    {
        if (tcb->fsm_state == CLOSE_WAIT || tcb->fsm_state == CLOSED)
        {
            LOG_DEBUG("[utcp_recv] Peer initialized shutdown. Unlocking the TCB for UTCP FD %i...", tcb->fd);
            pthread_mutex_lock(&tcb->lock);
            return 0;
        }

        LOG_DEBUG("[utcp_recv] RX buffer is empty. Client going to sleep for UTCP FD %i...", tcb->fd);
        pthread_cond_wait(&tcb->conn_cond, &tcb->lock);
    }

    size_t rx_used = ring_buf_used(&tcb->rx_buf);

    if (app_buf_len > rx_used)
    {
        LOG_DEBUG("[utcp_recv] Received data can fit into app's buffer for UTCP FD %i", tcb->fd);
        app_buf_len = rx_used; // put all of the data into the buffer since it fits
    }
    
    ring_buf_read(&tcb->rx_buf, buf, app_buf_len);

    tcb->rcv_wnd = ring_buf_free(&tcb->rx_buf);

    LOG_DEBUG("[utcp_recv] Unlocking the TCB for UTPC FD %i...", tcb->fd);
    pthread_mutex_unlock(&tcb->lock);
    return app_buf_len;
}


static void tcp_output(tcb_t *snder_tcb, int snder_sock) 
{
    uint32_t unacked_bytes_in_flight = snder_tcb->snd_nxt - snder_tcb->snd_una; // bytes that have been sent (in flight) (on the wire)
    size_t buffered_bytes = ring_buf_used(&snder_tcb->tx_buf); // data in tx_buf waiting to be sent
    
    int fd = snder_tcb->fd;
    /**
     * The effective window is the max amount of unACKed data that can be in-flight at
     * once. It is limited by either the receiver's capacity or the network's capacity.
     */
    uint32_t effective_wnd = MIN(snder_tcb->snd_wnd, snder_tcb->snd_cwnd);

    if (unacked_bytes_in_flight >= effective_wnd) {
        LOG_DEBUG("[tcp_output] There is no room to send more data for UTCP FD %i", fd);
        return; // no room to send more data.
    }
    uint32_t usable_wnd = effective_wnd - unacked_bytes_in_flight; // how many bytes can be sent
    
    LOG_DEBUG(
        "[tcp_output] \nthere are unACKed bytes in flight: %u,  \
        \n%zu bytes in the TX buffer waiting to be sent,        \
        \nand the max allowed unACKed bytes in flight is %u     \
        for UTCP FD %i",                                        \
        unacked_bytes_in_flight, buffered_bytes, effective_wnd, fd \
    );

    // look for unsent data in the TX buffer
    if (buffered_bytes > unacked_bytes_in_flight)
    { // true = new data is in the buffer that has not be sent out
        size_t unsent_bytes = buffered_bytes - unacked_bytes_in_flight;
        
        // We can only send what we have (bounded by the usable window)
        uint32_t bytes_to_send = MIN((uint32_t)unsent_bytes, usable_wnd); 

        LOG_DEBUG("[tcp_output] There are %zu unsent bytes, but only %u bytes can be sent at a time currently for UTCP FD %i", unsent_bytes, bytes_to_send, fd);

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
            LOG_DEBUG("[tcp_output] %u bytes were sent, there are now %u bytes left to send, and there are %u unACKed bytes in flight for UTCP FD %i", len, bytes_to_send, unacked_bytes_in_flight, fd);
        }
    }
}

