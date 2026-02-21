#include <utcp/server.h>

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <utcp/api/api.h>
#include <utcp/api/conn.h>
#include <utcp/api/data.h>
#include <utcp/api/rx_dgram.h>
#include <utcp/api/tx_dgram.h>
#include <utcp/api/ring_buffer.h>

#include <utils/printable.h>
#include <utils/err.h>

#include <tcp/hndshk_fsm.h>
#include <tcp/tcb.h>
#include <tcp/tcb_queue.h>

int main(void) {
    api_t *global = api_instance();
    listen_args_t *args = malloc(sizeof(listen_args_t));
    if (!args)
        err_sys("[Server, main] Failed to allocate args");
    pthread_t listen_thread;

    init_server(args, global);

    if (utcp_listen(args->utcp_fd, MAX_BACKLOG) != 0)
        err_sys("[Server, main] Error in utcp_listen");

    printf("[Server, main] Spawning listener thread...\n");
    if (pthread_create(&listen_thread, NULL, begin_listen, args) != 0)
        err_sys("[Server, main] Failed to create listener thread");

    tcb_t *listen_tcb = get_tcb(args->utcp_fd);

    while(1) 
    { // main thread -- pretend to be the application

        struct sockaddr_in client_info;
        int new_conn_fd = utcp_accept(listen_tcb, &client_info);
        tcb_t *new_tcb = get_tcb(new_conn_fd);

        if(new_tcb->fsm_state == ESTABLISHED)
        {
        printf("Sending a response to the test packet of data...\n");
        char *words = "Hi, this is my response\n";
        size_t len = strlen(words);
        size_t written = utcp_send(new_tcb, words, len);

        // determine how much we can send
        uint32_t flight = new_tcb->snd_nxt - new_tcb->snd_una; // bytes in flight
        printf("[utcp_recv] rcv_wnd update: %u\n", new_tcb->rcv_wnd);
        uint32_t wnd = new_tcb->rcv_wnd;
        uint32_t available = ring_buf_used(&new_tcb->tx_buf) - flight;
        uint32_t to_send = MIN(available, wnd);

        uint8_t tmp[MSS];
        len = MIN(MSS, available);

        ring_buf_peek(&new_tcb->tx_buf, tmp, len);
        send_dgram(args->udp_fd, new_tcb, tmp, len, 0);
        print_tcb(new_tcb);
        sleep(10000);
        }
    }
    free(args);
    return 0;
}

static void init_server(listen_args_t *args, api_t *global)
{
    struct sockaddr_in server = {
    .sin_family = AF_INET,
    .sin_port = htons(global->server_utcp_port),
    .sin_addr.s_addr = inet_addr("127.0.0.1"),
    };

    args->udp_fd = bind_UDP_sock(global->server_port);
    args->utcp_fd = bind_UTCP_sock(&server);
    printf("[init_server] utcp fd: %u\n", args->utcp_fd);
}


void* begin_listen(void *arg)
{
    printf("[Server] Listen thread running...\n");

    //api_t *global = api_instance();
    listen_args_t *args = (listen_args_t*)arg;
    //tcb_t         *listen_tcb = get_tcb(args->utcp_fd);
    //uint8_t       rcvbuf[BUF_SIZE]; // TODO replace with SYN queue and accept queue

    //struct tcphdr      *hdr; // TCP header in datagram's payload
    //struct sockaddr_in from; // store info on who sent the datagram
    //uint8_t*           data; // data that comes after TCP header in payload
    //ssize_t            data_len; // num bytes of data
    //ssize_t            rcvsize; // num bytes of payload (TCP header + data)
    
    //ring_buf_init(&listen_tcb->rx_buf);
    //ring_buf_init(&listen_tcb->tx_buf);
    //update_fsm(args->utcp_fd, LISTEN);

    while (1) {
        ssize_t rcvsize = rcv_dgram(args->udp_fd, BUF_SIZE);

        if (rcvsize < 0)
            err_sys("[Server, listen thread] Error receiving packet");

        //deserialize_utcp_packet(rcvbuf, rcvsize, &hdr, &data, &data_len);
        //print_tcb(listen_tcb);
    }
    return 0;
}

int utcp_listen(int utcp_fd, int backlog)
{
    tcb_t *tcb = get_tcb(utcp_fd);
    if (!tcb) return -1;

    if (backlog <= 0)
        backlog = 1;
    else
        backlog = MIN(backlog, MAX_BACKLOG);

    // Initialize queues
    memset(&tcb->syn_q, 0, sizeof(tcb_queue_t));
    tcb->syn_q.tcbs = calloc(backlog, sizeof(tcb_t*));

    if (!tcb->syn_q.tcbs)
        err_sys("[utcp_listen] Failed to allocate the SYN queue");

    pthread_mutex_init(&tcb->syn_q.lock, NULL);
    tcb->syn_q.backlog = backlog;
    // (No condition variable needed for SYN queue, as no one blocks waiting for it)

    memset(&tcb->accept_q, 0, sizeof(tcb_queue_t));
    tcb->accept_q.tcbs = calloc(backlog, sizeof(tcb_t*));

    if (!tcb->accept_q.tcbs)
    {
        free(tcb->syn_q.tcbs);
        err_sys("[utcp_listen] Failed to allocate the accept queue");
    }
    
    pthread_mutex_init(&tcb->accept_q.lock, NULL);
    pthread_cond_init(&tcb->accept_q.cond, NULL);

    tcb->accept_q.backlog = backlog;

    update_fsm(utcp_fd, LISTEN);

    //printf("\nPopulated the listen socket:\n");
    //print_tcb(tcb);

    return 0;   
}


int utcp_accept(tcb_t *listen_tcb, struct sockaddr_in *client_addr)
{
    if (!listen_tcb || listen_tcb->fsm_state != LISTEN) {
        printf("[utcp_accept] invalid socket\n");
        return -1;
    }
    //print_tcb(listen_tcb);

    pthread_mutex_lock(&listen_tcb->accept_q.lock);
    
    // Block until the queue is not empty
    while (listen_tcb->accept_q.count == 0) {
        // This unlocks the mutex and puts the thread to sleep.
        // It wakes up and re-locks when rcv_ack() calls pthread_cond_signal.
        pthread_cond_wait(&listen_tcb->accept_q.cond, &listen_tcb->accept_q.lock);
    }

    // Pop the established connection off the queue
    tcb_t *established_tcb = dequeue_tcb(&listen_tcb->accept_q);
    pthread_mutex_unlock(&listen_tcb->accept_q.lock);

    // Populate the client info so the application knows who connected
    if (client_addr) {
        client_addr->sin_family = AF_INET;
        client_addr->sin_addr.s_addr = htonl(established_tcb->fourtuple.dest_ip);
        client_addr->sin_port = htons(established_tcb->dest_udp_port); // or dest_port depending on your API
    }
    return established_tcb->fd;
}
