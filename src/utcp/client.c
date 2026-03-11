#include <utcp/client.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <unistd.h>

#include <tcp/hndshk_fsm.h>
#include <utils/err.h>
#include <utils/logger.h>
#include <utils/printable.h>
#include <utcp/api/api.h>
#include <utcp/api/conn.h>
#include <utcp/api/data.h>
#include <utcp/rx/rx_dgram.h>
#include <utcp/api/tx_dgram.h>

#include <zlog.h>


_Thread_local const char* current_thread_cat = "main_thread";

static void init_client(socket_fds *args)
{
    api_t *global = api_instance();

    args->udp_fd = bind_udp_sock(global->client_udp_port);
    global->udp_fd = args->udp_fd;

    struct sockaddr_in client = {
        .sin_family = AF_INET, 
        .sin_port = htons(776), 
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    args->utcp_fd = bind_utcp_sock(&client);
    LOG_INFO("[init_client] UDP & UTCP Sockets Initialized. UDP fd=%u,  Listen UTCP fd=%u\n", ntohs(args->udp_fd), args->utcp_fd);
}


void* begin_rcv(void *arg)
{
    current_thread_cat = "receive_thread";
    LOG_INFO("[begin_rcv] Receive thread running...");

    socket_fds *args = (socket_fds*)arg;

    while (1)
    {
        ssize_t rcv_size = rcv_dgram(args->udp_fd, BUF_SIZE);
        if (rcv_size < 0)
            err_sys("[being_rcv] Error: failed to receive packet");
    }
    return NULL;
}


static int utcp_connect(int udp_fd, const struct sockaddr_in *dest_addr)
{
    api_t *global = api_instance();

    LOG_INFO("[utcp_connect] Creating a new TCB for the application's connection request...");

    tcb_t *new_tcb = alloc_new_tcb();

    pthread_mutex_lock(&new_tcb->lock);
    //LOG_INFO("[utcp_connect] Locked the TCB...");
    int utcp_fd = new_tcb->fd;
    new_tcb->src_udp_fd = udp_fd;

    //new_tcb->fourtuple.source_port = 49152 + (rand() % 16384); // Ephemeral port
    new_tcb->fourtuple.source_port = 49152 + (utcp_fd);
    new_tcb->fourtuple.source_ip   = ntohl(global->client.sin_addr.s_addr);
    new_tcb->fourtuple.dest_port   = ntohs(dest_addr->sin_port);
    new_tcb->fourtuple.dest_ip     = ntohl(dest_addr->sin_addr.s_addr);
    
    new_tcb->dest_udp_port         = global->server_udp_port;
    new_tcb->src_udp_port          = global->client_udp_port;

    new_tcb->iss = 100;
    new_tcb->snd_una = new_tcb->iss;
    new_tcb->snd_nxt = new_tcb->iss;
    new_tcb->snd_max = new_tcb->iss;
    new_tcb->rcv_wnd = BUF_SIZE;
    update_fsm(utcp_fd, SYN_SENT);

    log_tcb(new_tcb, "[utcp_connect] Finished initializing variables for the new TCB:");

    LOG_INFO("[utcp_connect] Sending SYN for UTCP FD %i...", utcp_fd);
    int SYN_dgram = send_dgram(new_tcb);

    while (new_tcb->fsm_state != ESTABLISHED) // block and wait for SYN-ACK
        pthread_cond_wait(&new_tcb->conn_cond, &new_tcb->lock);

    //LOG_INFO("[utcp_connect] Unlocking the TCB...");
    pthread_mutex_unlock(&new_tcb->lock);

    LOG_INFO("[utcp_connect] 3WHS is complete.");
    return utcp_fd;
}

static int spawn_threads(socket_fds *args)
{
    pthread_t rcv_thread;
    pthread_t ticker_thread;

    LOG_INFO("[spawn_threads] Spawning receiver thread...");
    if (pthread_create(&rcv_thread, NULL, begin_rcv, args) != 0)
    {
        LOG_FATAL("[spawn_threads] Failed to create receiver thread");
        return -1;
    }

    LOG_INFO("[spawn_threads] Spawning ticker thread...");
    if (pthread_create(&ticker_thread, NULL, utcp_ticker_thread, NULL) != 0)
    {
        LOG_FATAL("[spawn_threads] Failed to create ticker thread");
        return -1;
    }

    return 0;
}


int main(void) {
    if (init_zlog("zlog_client.conf") != 0) // initialize logger
        err_sys("Error initializing zlog");

    api_t *global = api_instance();

    socket_fds *args = malloc(sizeof(socket_fds));
    if (!args)
        err_sys("[Client, main] Failed to allocate args");

    init_client(args);
    
    if(spawn_threads(args) != 0)
    {
        err_sys("[Client] Error during thread creation");
    }


    /* pretend to be a client app */
    args->utcp_fd = utcp_connect(args->udp_fd, &global->server);
    if (args->utcp_fd < 0)
        err_sock(args->udp_fd, "[Client] Failed to connect");
            
    FILE *fp = fopen("./test_rcvd.txt", "wb"); // wb to ensure it's an exact copy
    if (!fp) {
        err_sys("[Client App] Failed to create destination file");
    }
    size_t file_size_bytes = 1000000000; // 1GB
    uint8_t *app_rcv_buf = malloc(BUF_SIZE + 1);
    size_t total_received = 0;

    while(total_received < file_size_bytes)
    {   
        ssize_t bytes_rcvd = utcp_recv(args->utcp_fd, app_rcv_buf, BUF_SIZE);
        if (bytes_rcvd > 0)
        {
            fwrite(app_rcv_buf, 1, bytes_rcvd, fp);
            fflush(fp); // forces the OS to write to the txt file immediately
            total_received += (size_t)bytes_rcvd;
            LOG_INFO("[Client App] Wrote %zd bytes to disk. Total: %zu", bytes_rcvd, total_received);
        }
        if (bytes_rcvd < 0) {
            LOG_ERROR("[Client App] Error receiving data.");
            break; 
        }
    }

    tcb_t *active_tcb = get_tcb(args->utcp_fd);
    while(active_tcb->rx_tail - active_tcb->rx_head > 0)
        usleep(100000);
    sleep(2);

    fclose(fp);
    LOG_INFO("[Client App] Finished. Received %zu bytes total", total_received);

    free(args);
    free(app_rcv_buf);
    return 0;
}
