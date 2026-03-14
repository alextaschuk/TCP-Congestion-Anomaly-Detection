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
#include <utcp/api/tx_dgram.h>


ssize_t utcp_send(int utcp_fd, int udp_fd, const void *buf, size_t payload_len)
{
    tcb_t *snder_tcb = get_tcb(utcp_fd);
    if (!snder_tcb)
    {
        LOG_ERROR("[utcp_recv] Invalid UTCP fd");
        return -1;
    }

    const uint8_t *data_ptr = (const uint8_t *)buf;
    size_t remaining = payload_len;
    size_t sent = 0; // # of bytes sent

    pthread_mutex_lock(&snder_tcb->lock);

    while (remaining > 0)
    {
        uint32_t curr_buffered = snder_tcb->tx_tail - snder_tcb->tx_head;
        uint32_t free_space = BUF_SIZE - curr_buffered; // how many bytes can we add to the TX buffer?
        //LOG_INFO("[utcp_send] Num bytes in TX buf: %u, amount of free space in TX: %u", curr_buffered, free_space);

        if (free_space == 0)
        {
            //LOG_DEBUG("[utcp_send] TX buffer is full for tcb %i. App thread will block until there is room to send...", snder_tcb->fd);
            pthread_cond_wait(&snder_tcb->conn_cond, &snder_tcb->lock);
            //LOG_DEBUG("[utcp_send] App thread was woken up because TX buffer has room for tcb %i.", snder_tcb->fd);

            if (snder_tcb->fsm_state != ESTABLISHED)
            { // network's connection dropped; unlock and exit.
                LOG_DEBUG("[utcp_send] Network connection dropped. Unlocking the TCB for UTCP FD %i", snder_tcb->fd);
                pthread_mutex_unlock(&snder_tcb->lock);
                break;
            }
            continue;
        }
        
        // write as much data as we can fit 
        size_t to_write = MIN(remaining, free_space);
        //LOG_INFO("[utcp_send] %zu bytes can be sent", to_write);

        for (size_t i = 0; i < to_write; i++)
        { // write data from buf into TX byte-by-byte
            snder_tcb->tx_buf[(snder_tcb->tx_tail + i) % BUF_SIZE] = data_ptr[i];
        }

        snder_tcb->tx_tail += to_write; // move the tail forward.
        data_ptr += to_write; // go to the next chunk of data.
        remaining -= to_write;

        LOG_DEBUG("[utcp_send] Added %zu bytes to send buffer on fd %i. %zu bytes remaining.", to_write, snder_tcb->fd, remaining);

        sent += send_dgram(snder_tcb); // try to send the data to the peer
    }

    //LOG_DEBUG("[utcp_send] Unlocking the TCB for UTCP FD %i", snder_tcb->fd);
    pthread_mutex_unlock(&snder_tcb->lock);
    
    return (ssize_t)payload_len;
}


ssize_t utcp_recv(int utcp_fd, uint8_t *buf, size_t app_buf_len)
{
    tcb_t *tcb = get_tcb(utcp_fd);
    if (!tcb)
    {
        LOG_ERROR("[utcp_recv] Invalid UTCP fd");
        return -1;
    }

    pthread_mutex_lock(&tcb->lock);
    //LOG_DEBUG("[utcp_recv] Locked the TCB for fd=%i...", tcb->fd);

    while (tcb->rx_head == tcb->rx_tail)
    { // nothing to read
        if (tcb->fsm_state == CLOSE_WAIT || tcb->fsm_state == CLOSED)
        {
            LOG_DEBUG("[utcp_recv] Peer initialized shutdown. Unlocking the TCB for UTCP FD %i...", tcb->fd);
            pthread_mutex_unlock(&tcb->lock);
            return 0;
        }

        //LOG_DEBUG("[utcp_recv] RX buffer is empty for UTCP FD %i. App thread will block until there is room to send...", tcb->fd);
        pthread_cond_wait(&tcb->conn_cond, &tcb->lock);
        //LOG_DEBUG("[utcp_recv] App thread unblock because data was added to the RX buffer. Attempting to read from it...", tcb->fd);
    }

    /* Read either `app_buf_len` bytes from the RX buffer, or the entire buffer; whichever is smaller. */
    uint32_t avail_bytes_to_read = tcb->rx_tail - tcb->rx_head;
    size_t num_bytes_to_read = MIN(app_buf_len, (size_t)avail_bytes_to_read);

    for (size_t i = 0; i < num_bytes_to_read; i++)
    {
        buf[i] = tcb->rx_buf[(tcb->rx_head + i) % BUF_SIZE];
    }

    tcb->rx_head += num_bytes_to_read;

    /**
     * When the application has read the payload (i.e., the RX buffer), we can free up the receive
     * window that's advertised to the sender by recalculating the app's rcv_wnd. ooo_bytes are
     * reserved for out-of-order segments that will drain into the RX buffer.
     */
    uint32_t bytes_in_buf = tcb->rx_tail - tcb->rx_head;
    tcb->rcv_wnd = BUF_SIZE - bytes_in_buf - tcb->ooo_bytes;

    // Silly window prevention with Classic Clark's algorithm: only send window update when
    // we can offer at least min(MSS, BUF_SIZE / 2) worth of new space.
    uint32_t sws_threshold = MIN(MSS, BUF_SIZE / 2);
    if (tcb->rcv_wnd >= sws_threshold || (tcb->rcv_wnd < sws_threshold && avail_bytes_to_read == BUF_SIZE))
    {
        //LOG_DEBUG("SWS triggered on fd %d: Sending window update (rcv_wnd=%u)", utcp_fd, tcb->rcv_wnd);
        tcb->t_flags |= F_ACKNOW;
        send_dgram(tcb);
    }

    //LOG_DEBUG("[utcp_recv] Unlocking the TCB for fd=%i...", tcb->fd);
    pthread_mutex_unlock(&tcb->lock);

    return (int)num_bytes_to_read;
}
