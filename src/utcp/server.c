#include <utcp/server.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>

#include <tcp/hndshk_fsm.h>
#include <tcp/tcb.h>
#include <tcp/tcb_queue.h>
#include <utils/err.h>
#include <utils/logger.h>
#include <utils/printable.h>
#include <utcp/api/api.h>
#include <utcp/api/conn.h>
#include <utcp/api/data.h>
#include <utcp/api/ring_buffer.h>
#include <utcp/rx/rx_dgram.h>
#include <utcp/api/tx_dgram.h>
#include <utcp/api/utcp_timers.h>

#include <zlog.h>


_Thread_local const char* current_thread_cat = "main_thread";

//make clean && make && clear && clear && ./server_app

tcb_t *active_tcbs[MAX_CONNECTIONS]; // global array of TCBs w/ ESTABLISHED state


static void init_server(socket_fds *args, api_t *global)
{
    struct sockaddr_in server = {
    .sin_family = AF_INET,
    .sin_port = htons(global->server_utcp_port),
    .sin_addr.s_addr = inet_addr("127.0.0.1"),
    };

    args->udp_fd = bind_UDP_sock(global->server_port);
    args->utcp_fd = bind_UTCP_sock(&server);

    LOG_INFO("[init_server] UDP & UTCP Sockets Initialized. UDP FD: %u,  Listen UTCP FD: %u", args->udp_fd, args->utcp_fd);
}


void* utcp_listen_thread(void *arg)
{
    current_thread_cat = "listen_thread";
    LOG_INFO("[utcp_listen_thread] Listen thread running...");
    socket_fds *args = (socket_fds*)arg;

    while (1)
    {
        ssize_t rcvsize = rcv_dgram(args->udp_fd, BUF_SIZE);
        if (rcvsize < 0)
            err_sys("[Server, listen thread] Error receiving packet");
    }
    return 0;
}


int utcp_listen(int utcp_fd, int backlog)
{
    tcb_t *tcb = get_tcb(utcp_fd);
    if (!tcb)
        return -1;

    if (backlog <= 0)
        backlog = 1;
    else
        backlog = MIN(backlog, MAX_BACKLOG);

    /* initialize SYN queue */
    memset(&tcb->syn_q, 0, sizeof(tcb_queue_t));
    tcb->syn_q.tcbs = calloc(backlog, sizeof(tcb_t*));

    if (!tcb->syn_q.tcbs)
        err_sys("[utcp_listen] Failed to allocate the SYN queue");

    pthread_mutex_init(&tcb->syn_q.lock, NULL);
    tcb->syn_q.backlog = backlog;

    /* initialize accept queue */
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
    return 0;   
}


int utcp_accept(tcb_t *listen_tcb, struct sockaddr_in *client_addr)
{

    if (!listen_tcb || listen_tcb->fsm_state != LISTEN)
    {
        err_sock(listen_tcb->src_udp_port, "[utcp_accept] Invalid socket");
        return -1;
    }

    LOG_INFO("[utcp_accept] Locking the accept queue");
    pthread_mutex_lock(&listen_tcb->accept_q.lock);
    
    // block until the queue is not empty (TCB added via rx_dgram())
    while (listen_tcb->accept_q.count == 0)
    {
        pthread_cond_wait(&listen_tcb->accept_q.cond, &listen_tcb->accept_q.lock);
    }

    // pop the established connection off the queue
    tcb_t *established_tcb = dequeue_tcb(&listen_tcb->accept_q);

    LOG_INFO("[utcp_accept] Unlocking the accept queue");
    pthread_mutex_unlock(&listen_tcb->accept_q.lock);

    // populate the client info so the app knows who is connected
    if (client_addr)
    {
        client_addr->sin_family = AF_INET;
        client_addr->sin_addr.s_addr = htonl(established_tcb->fourtuple.dest_ip);
        client_addr->sin_port = htons(established_tcb->dest_udp_port);
    }

    // initialize the RX and TX buffers
    ring_buf_init(&established_tcb->rx_buf);
    ring_buf_init(&established_tcb->tx_buf);

    return established_tcb->fd;
}


static int spawn_threads(socket_fds *args)
{
    pthread_t listen_thread;
    pthread_t ticker_thread;

    LOG_INFO("[spawn_threads] Spawning listener thread...");
    if (pthread_create(&listen_thread, NULL, utcp_listen_thread, args) != 0)
    {
        LOG_ERROR("[spawn_threads] Failed to create listener thread");
        return -1;
    }

    LOG_INFO("[spawn_threads] Spawning ticker thread...");
    if (pthread_create(&ticker_thread, NULL, utcp_ticker_thread, NULL) != 0)
    {
        LOG_INFO("[spawn_threads] Failed to create ticker thread");
        return -1;
    }
    return 0;
}


int main(void) {
    if (init_zlog("zlog_server.conf") != 0) // initialize logger
        err_sys("Error initializing zlog");

    api_t *global = api_instance();

    socket_fds *args = malloc(sizeof(socket_fds));
    if (!args)
        err_sys("[Server, main] Failed to allocate args");

    init_server(args, global);

    if (utcp_listen(args->utcp_fd, MAX_BACKLOG) != 0)
        err_sys("[Server, main] Error in utcp_listen");

    if(spawn_threads(args) != 0)
        err_sys("[Server] Error during thread creation");

    tcb_t *listen_tcb = get_tcb(args->utcp_fd);

    struct sockaddr_in client_info;
    int new_conn_fd = utcp_accept(listen_tcb, &client_info);
    tcb_t *new_tcb = get_tcb(new_conn_fd);
    new_tcb->src_udp_port = args->udp_fd;
    while(1) 
    { // main thread -- pretend to be the application
        if(new_tcb->fsm_state == ESTABLISHED)
        {
        //char *words = "This is a test payload from the server";
        char *words = "Lorem ipsum dolor sit amet consectetur adipiscing elit. Quisque faucibus ex sapien vitae pellentesque sem placerat. In id cursus mi pretium tellus duis convallis. Tempus leo eu aenean sed diam urna tempor. Pulvinar vivamus fringilla lacus nec metus bibendum egestas. Iaculis massa nisl malesuada lacinia integer nunc posuere. Ut hendrerit semper vel class aptent taciti sociosqu. Ad litora torquent per conubia nostra inceptos himenaeos. \nLorem ipsum dolor sit amet consectetur adipiscing elit. Quisque faucibus ex sapien vitae pellentesque sem placerat. In id cursus mi pretium tellus duis convallis. Tempus leo eu aenean sed diam urna tempor. Pulvinar vivamus fringilla lacus nec metus bibendum egestas. Iaculis massa nisl malesuada lacinia integer nunc posuere. Ut hendrerit semper vel class aptent taciti sociosqu. Ad litora torquent per conubia nostra inceptos himenaeos. \nLorem ipsum dolor sit amet consectetur adipiscing elit. Quisque faucibus ex sapien vitae pellentesque sem placerat. In id cursus mi pretium tellus duis convallis. Tempus leo eu aenean sed diam urna tempor. Pulvinar vivamus fringilla lacus nec metus bibendum egestas. Iaculis massa nisl malesuada lacinia integer nunc posuere. Ut hendrerit semper vel class aptent taciti sociosqu. Ad litora torquent per conubia nostra inceptos himenaeos.";
        size_t total_len = strlen(words);
        size_t total_sent = 0;

        size_t written = utcp_send(new_tcb, args->udp_fd, words, total_len);
        LOG_INFO("[main] Sent a test packet of data to the server with a payload size of %zu bytes", written);

        if (written < 0)
        {
            LOG_ERROR("[main] Connection closed or error occured during send.");
            break;
        }
        total_sent += written;

        LOG_INFO("Sent %zd bytes. Total sent so far: %zu / %zu", written, total_sent, total_len);
        sleep(2);
        }
    }
    free(args); // unreachable
    return 0;
}
