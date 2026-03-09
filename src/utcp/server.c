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
#include <utcp/rx/rx_dgram.h>
#include <utcp/api/tx_dgram.h>
#include <utcp/api/utcp_timers.h>

#include <zlog.h>
#define CHUNK_SIZE 65536 // 64KB

_Thread_local const char* current_thread_cat = "main_thread";

//make clean && make && clear && clear && ./server_app

tcb_t *active_tcbs[MAX_CONNECTIONS]; // global array of TCBs w/ ESTABLISHED state


static void init_server(socket_fds *args, api_t *global)
{
    args->udp_fd = bind_udp_sock(global->server_port);
    global->udp_fd = args->udp_fd;

    struct sockaddr_in server = {
    .sin_family = AF_INET,
    .sin_port = htons(global->server_utcp_port),
    .sin_addr.s_addr = inet_addr("127.0.0.1"),
    };

    args->utcp_fd = bind_utcp_sock(&server);

    LOG_INFO("[init_server] UDP & UTCP Sockets Initialized. UDP FD=%u,  Listen UTCP FD=%u\n", ntohs(args->udp_fd), args->utcp_fd);
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
            err_sys("[Server, listen thread] Error receiving packet\n");
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
        err_sys("[utcp_listen] Failed to allocate the SYN queue\n");

    pthread_mutex_init(&tcb->syn_q.lock, NULL);
    tcb->syn_q.backlog = backlog;

    /* initialize accept queue */
    memset(&tcb->accept_q, 0, sizeof(tcb_queue_t));
    tcb->accept_q.tcbs = calloc(backlog, sizeof(tcb_t*));

    if (!tcb->accept_q.tcbs)
    {
        free(tcb->syn_q.tcbs);
        err_sys("[utcp_listen] Failed to allocate the accept queue\n");
    }
    
    pthread_mutex_init(&tcb->accept_q.lock, NULL);
    pthread_cond_init(&tcb->accept_q.cond, NULL);

    tcb->accept_q.backlog = backlog;

    update_fsm(utcp_fd, LISTEN);
    return 0;   
}


int utcp_accept(socket_fds *args, struct sockaddr_in *client_addr)
{
    tcb_t *listen_tcb = get_tcb(args->utcp_fd);
    if (!listen_tcb || listen_tcb->fsm_state != LISTEN)
    {
        err_sock(listen_tcb->src_udp_fd, "[utcp_accept] Invalid listen socket\n");
        return -1;
    }

    pthread_mutex_lock(&listen_tcb->accept_q.lock);
    LOG_DEBUG("[utcp_accept] Locked the accept queue and blocking until a connection is added.");
    
    // block until the queue is not empty (TCB added via rx_dgram())
    while (listen_tcb->accept_q.count == 0)
    {
        pthread_cond_wait(&listen_tcb->accept_q.cond, &listen_tcb->accept_q.lock);
    }

    // pop the established connection off the queue
    tcb_t *established_tcb = dequeue_tcb(&listen_tcb->accept_q);

    LOG_DEBUG("[utcp_accept] An established connection with fd=%i has been added. Unlocking the accept queue...\n");
    pthread_mutex_unlock(&listen_tcb->accept_q.lock);

    // populate the client info so the app knows who is connected
    if (client_addr)
    {
        client_addr->sin_family = AF_INET;
        client_addr->sin_addr.s_addr = htonl(established_tcb->fourtuple.dest_ip);
        client_addr->sin_port = htons(established_tcb->dest_udp_port);
    }

    established_tcb->src_udp_fd = args->udp_fd;

    return established_tcb->fd;
}


static int spawn_threads(socket_fds *args)
{
    pthread_t listen_thread;
    pthread_t ticker_thread;

    LOG_INFO("[spawn_threads] Spawning listener thread...\n");
    if (pthread_create(&listen_thread, NULL, utcp_listen_thread, args) != 0)
    {
        LOG_ERROR("[spawn_threads] Failed to create listener thread\n");
        return -1;
    }

    LOG_INFO("[spawn_threads] Spawning ticker thread...");
    if (pthread_create(&ticker_thread, NULL, utcp_ticker_thread, NULL) != 0)
    {
        LOG_INFO("[spawn_threads] Failed to create ticker thread\n");
        return -1;
    }
    return 0;
}


int main(void)
{
    if (init_zlog("zlog_server.conf") != 0) // initialize logger
        err_sys("Error initializing zlog");

    api_t *global = api_instance();

    socket_fds *args = malloc(sizeof(socket_fds));
    if (!args)
        err_sys("[Server, main] Failed to allocate args");

    init_server(args, global);
    LOG_DEBUG("[MAIN] args after init: %d, %d", args->udp_fd, args->utcp_fd);

    if (utcp_listen(args->utcp_fd, MAX_BACKLOG) != 0)
        err_sys("[Server, main] Error in utcp_listen");

    if(spawn_threads(args) != 0)
        err_sys("[Server] Error during thread creation");

    tcb_t *listen_tcb = get_tcb(args->utcp_fd);

    pthread_mutex_lock(&listen_tcb->lock);
    listen_tcb->src_udp_fd = args->udp_fd;
    pthread_mutex_unlock(&listen_tcb->lock);

    struct sockaddr_in client_info;
    int new_utcp_fd = utcp_accept(args, &client_info);
    tcb_t *new_tcb = get_tcb(new_utcp_fd);

    pthread_mutex_lock(&new_tcb->lock);
    new_tcb->src_udp_fd = args->udp_fd;
    pthread_mutex_unlock(&new_tcb->lock);

    LOG_INFO("[Server App] opening tgg.txt...");

    //FILE *fp = fopen("/Users/alex/Desktop/directed-study/jungle_book.txt", "rb"); // rb to prevent OS from changing newline characters
    FILE *fp = fopen("/Users/alex/Desktop/directed-study/tgg.txt", "rb"); // rb to prevent OS from changing newline characters
    if (!fp)
        err_sys("[Server App] Failed to open text file");
            
    uint8_t *snd_buf = malloc(BUF_SIZE);
    size_t bytes_read = 0;
    size_t total_file_bytes = 0;

    //while ((bytes_read = fread(snd_buf, 1, sizeof(snd_buf), fp)) > 0) 
    while((bytes_read = fread(snd_buf, 1, CHUNK_SIZE, fp)) > 0)
    {
        ssize_t sent = utcp_send(new_utcp_fd, args->udp_fd, snd_buf, bytes_read);
        if (sent < 0)
        {
            LOG_ERROR("[Server App] Connection dropped during file transfer.");
            break;
        }

        total_file_bytes += sent;
    }
        // Wait for the background thread to finish draining the TX buffer before exiting!
        tcb_t *active_tcb = get_tcb(new_utcp_fd);
        while(active_tcb->tx_head > 0)
            usleep(100000);
        sleep(5); // Wait for final ACKs

        fclose(fp);
        LOG_INFO("[Server App] File queued successfully. Total bytes: %zu", total_file_bytes);

        free(args);
        return 0;
}
