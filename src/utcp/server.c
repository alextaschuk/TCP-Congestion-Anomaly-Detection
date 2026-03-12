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
//make clean && make && clear && clear && ./server_app

_Thread_local const char* current_thread_cat = "main_thread";


int utcp_listen(api_t *global, int backlog)
{
    tcb_t *tcb = get_tcb(global->utcp_fd);
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

    update_fsm(global->utcp_fd, LISTEN);
    return 0;   
}


int utcp_accept(api_t *global)
{
    tcb_t *listen_tcb = get_tcb(global->utcp_fd);
    if (!listen_tcb || listen_tcb->fsm_state != LISTEN)
    {
        err_sock(listen_tcb->src_udp_fd, "[utcp_accept] Invalid listen socket");
        return -1;
    }

    pthread_mutex_lock(&listen_tcb->accept_q.lock);
    //LOG_DEBUG("[utcp_accept] Locked the accept queue and blocking until a connection is added.");
    
    // block until the queue is not empty (TCB added via rx_dgram())
    while (listen_tcb->accept_q.count == 0)
    {
        pthread_cond_wait(&listen_tcb->accept_q.cond, &listen_tcb->accept_q.lock);
    }

    // pop the established connection off the queue
    tcb_t *established_tcb = dequeue_tcb(&listen_tcb->accept_q);

    //LOG_DEBUG("[utcp_accept] An established connection with fd=%i has been added. Unlocking the accept queue...");
    pthread_mutex_unlock(&listen_tcb->accept_q.lock);

    established_tcb->src_udp_fd = global->udp_fd;

    return established_tcb->fd;
}


int main(void)
{
    if (init_zlog("zlog_server.conf") != 0) // initialize logger
        err_sys("Error initializing zlog");

    api_t *global = api_instance();
    global->utcp_fd = 0; // listen socket will always have fd of 0
    int utcp_fd = init_utcp_sock();

    struct sockaddr_in server = {
        .sin_family = AF_INET, 
        .sin_port = htons(332), // UTCP port 
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    
    bind_utcp_sock(utcp_fd, &server);
    
    //bind_udp_sock(1515);
    //global->utcp_fd = bind_utcp_sock(&global->server);
    //init_host(global, server);
    log_tcb(get_tcb(utcp_fd), "Post init TCB:");

    if (utcp_listen(global, MAX_BACKLOG) != 0)
        err_sys("[main] Error in utcp_listen");

    if(spawn_threads(global) != 0)
        err_sys("[main] Error during thread creation");
    
    tcb_t *listen_tcb = get_tcb(utcp_fd);

    pthread_mutex_lock(&listen_tcb->lock);
    listen_tcb->src_udp_fd = global->udp_fd;
    pthread_mutex_unlock(&listen_tcb->lock);

    int new_utcp_fd = utcp_accept(global);
    tcb_t *new_tcb = get_tcb(new_utcp_fd);

    pthread_mutex_lock(&new_tcb->lock);
    new_tcb->src_udp_fd = global->udp_fd;
    pthread_mutex_unlock(&new_tcb->lock);

    FILE *fp = fopen("../test_file.txt", "rb"); // rb to prevent OS from changing newline characters
    if (!fp)
        err_sys("[Server App] Failed to open text file");
            
    size_t file_size_bytes = 10000000; // 10mb
    uint8_t *snd_buf = malloc(APP_BUF_SIZE);
    size_t bytes_read = 0;
    size_t total_file_bytes = 0;

    while((bytes_read = fread(snd_buf, 1, APP_BUF_SIZE, fp)) > 0)
    {
        ssize_t sent = utcp_send(new_utcp_fd, global->udp_fd, snd_buf, bytes_read);
        if (sent < 0)
        {
            LOG_ERROR("[Server App] Connection dropped during file transfer.");
            break;
        }

        total_file_bytes += sent;
        fflush(stdout);
    }

        fclose(fp);
        free(snd_buf);
        LOG_INFO("[Server App] File queued successfully. Total bytes: %zu", total_file_bytes);
        return 0;
}
