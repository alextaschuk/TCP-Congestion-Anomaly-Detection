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


void* begin_rcv(void *arg)
{
    current_thread_cat = "receive_thread";
    LOG_INFO("[begin_rcv] Receive thread running...");

    socket_fds *args = (socket_fds*)arg;

    while (1)
    {
        ssize_t rcv_size = rcv_dgram(args->udp_fd, BUF_SIZE);

        if (rcv_size < 0)
            err_sys("[being_rcv] Error: failed to receive packet\n");
    }
    return NULL;
}


static int utcp_connect(int udp_fd, const struct sockaddr_in *dest_addr)
{
    api_t *global = api_instance();

    LOG_INFO("[utcp_connect] Creating a new TCB for the application's connection request...");

    tcb_t *new_tcb = alloc_new_tcb();

    pthread_mutex_lock(&new_tcb->lock);
    LOG_INFO("[utcp_connect] Locked the TCB...");
    int utcp_fd = new_tcb->fd;
    new_tcb->src_udp_fd = udp_fd;

    //new_tcb->fourtuple.source_port = 49152 + (rand() % 16384); // Ephemeral port
    new_tcb->fourtuple.source_port = 49152 + (utcp_fd);
    new_tcb->fourtuple.source_ip   = htonl(INADDR_LOOPBACK);
    new_tcb->fourtuple.dest_port   = ntohs(dest_addr->sin_port);
    new_tcb->fourtuple.dest_ip     = ntohl(dest_addr->sin_addr.s_addr);
    new_tcb->dest_udp_port         = 4567;

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

    LOG_INFO("[utcp_connect] Unlocking the TCB...");
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
    pthread_detach(rcv_thread);


    LOG_INFO("[spawn_threads] Spawning ticker thread...");
    if (pthread_create(&ticker_thread, NULL, utcp_ticker_thread, NULL) != 0)
    {
        LOG_FATAL("[spawn_threads] Failed to create ticker thread");
        return -1;
    }
    pthread_detach(ticker_thread);

    return 0;
}


int main(void) {
    api_t *global = api_instance();
    
    if (init_zlog("zlog_client.conf") != 0) // initialize logger
        err_sys("Error initializing zlog");

    socket_fds *args = malloc(sizeof(socket_fds));
    if (!args)
        err_sys("[Client, main] Failed to allocate args");


    args->udp_fd = bind_udp_sock(global->client_port);
    global->udp_fd = args->udp_fd;

    if(spawn_threads(args) != 0)
    {
        err_sys("[Client] Error during thread creation");
    }

    struct sockaddr_in server = {
    .sin_family = AF_INET,
    .sin_port = htons(global->server_utcp_port),
    .sin_addr.s_addr = inet_addr("127.0.0.1"),
    };

    // pretend to be a client app
    LOG_INFO("[Client App] connecting to server via utcp_connect()");

    args->utcp_fd = utcp_connect(args->udp_fd, &server);
    if (args->utcp_fd < 0)
        err_sock(args->udp_fd, "[Client] Failed to connect");
    
    LOG_INFO("[utcp_connect] 3WHS complete.");

    //tcb_t *tcb = get_tcb(args->utcp_fd);

    LOG_INFO("[Client App] Opening 'tgg_rcvd.txt'...");
    //FILE *fp = fopen("/Users/alex/Desktop/directed-study/jungle_book_rcvd.txt", "wb"); // wb to ensure it's an exact copy

    FILE *tgg = fopen("/Users/alex/Desktop/directed-study/test_file.txt", "rb");
    long file_size_bytes = -1;
    if (fseek(tgg, 0L, SEEK_END) == 0) { // Move to the end
        file_size_bytes = ftell(tgg); // Get the position, which is the size

        LOG_INFO("[CLIENT] size of tgg: %ld", file_size_bytes);

        if (file_size_bytes == -1L)
            perror("Error getting file position");
    }

    if (fclose(tgg) != 0) {
        perror("Error closing file");
    }
    
    FILE *fp = fopen("/Users/alex/Desktop/directed-study/test_rcvd.txt", "wb"); // wb to ensure it's an exact copy
    if (!fp) {
        err_sys("[Client App] Failed to create destination file");
    }

    uint8_t *app_rcv_buf = malloc(BUF_SIZE + 1);
    size_t total_received = 0;
    //#define TARGET_SIZE (10 * 24 *24) // send 10mb total

    while(total_received < (size_t)file_size_bytes)
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
        else if (bytes_rcvd == 0) {
            // This triggers when the server sends a FIN flag
            LOG_INFO("[Client App] Server gracefully closed the connection.");
            break;
        }
    }

    tcb_t *active_tcb = get_tcb(args->utcp_fd);
    while(active_tcb->rx_tail - active_tcb->rx_head > 0)
        usleep(100000);
    sleep(2);

    fclose(fp);
    LOG_INFO("[Client App] Finished. Received %zu bytes total", total_received);

    free(args); // unreachable
    free(app_rcv_buf);
    return 0;
}
