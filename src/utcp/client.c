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
#include <utcp/api/ring_buffer.h>
#include <utcp/rx/rx_dgram.h>
#include <utcp/api/tx_dgram.h>

#include <zlog.h>


_Thread_local const char* current_thread_cat = "main_thread";

static void init_client(void *arg, api_t *global)
{
    socket_fds *args = (socket_fds*)arg;

    // TODO move these structs to globals
    struct sockaddr_in client = {
    .sin_family = AF_INET,
    .sin_port = htons(global->client_utcp_port),
    .sin_addr.s_addr = inet_addr("127.0.0.1"),
    };

    struct sockaddr_in server = {
    .sin_family = AF_INET,
    .sin_port = htons(global->server_utcp_port),
    .sin_addr.s_addr = inet_addr("127.0.0.1"),
    };

    args->udp_fd = bind_UDP_sock(global->client_port);
    args->utcp_fd = bind_UTCP_sock(&client);

    tcb_t *tcb = get_tcb(args->utcp_fd);
    tcb->fourtuple.dest_port = ntohs(server.sin_port); //dest UTCP port
    tcb->fourtuple.dest_ip = ntohl(server.sin_addr.s_addr); // dest IP
    tcb->dest_udp_port = 4567; // dest UDP port number
    
    tcb->iss = 100; // will be randomly chosen in future
    tcb->snd_una = tcb->iss; // hasn't been ack'd b/c SYN not yet sent
    tcb->snd_nxt = tcb->iss;
}


static void init_hndshk(void *arg)
{
    /* Create and configure TCP header */
    socket_fds *args = (socket_fds*)arg;
    tcb_t *tcb = get_tcb(args->utcp_fd);

    ring_buf_init(&tcb->rx_buf);
    ring_buf_init(&tcb->tx_buf);
    printf("intialized ring buffers");

    /* 3WHS */
    int SYN_dgram = send_dgram(tcb, args->udp_fd, NULL, 0, TH_SYN);
    update_fsm(args->utcp_fd, SYN_SENT);
    ssize_t ACK_dgram = rcv_dgram(args->udp_fd, BUF_SIZE);

    log_tcb(tcb, "[init_hndshk] TCB for new connection:");
}


void* begin_rcv(void *arg)
{
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

    /* create a new TCB for the client's connection request */
    tcb_t *new_tcb = alloc_new_tcb();
    int utcp_fd = new_tcb->fd;
    new_tcb->src_udp_port = udp_fd;

    LOG_INFO("[utcp_connect] Locking the TCB...");
    pthread_mutex_lock(&new_tcb->lock);
    //new_tcb->fourtuple.source_port = 49152 + (rand() % 16384); // Ephemeral port
    new_tcb->fourtuple.source_port = 49152 + (utcp_fd);
    new_tcb->fourtuple.source_ip   = htonl(INADDR_LOOPBACK);
    new_tcb->fourtuple.dest_port   = ntohs(dest_addr->sin_port);
    new_tcb->fourtuple.dest_ip     = ntohl(dest_addr->sin_addr.s_addr);
    new_tcb->dest_udp_port         = 4567;

    ring_buf_init(&new_tcb->rx_buf);
    ring_buf_init(&new_tcb->tx_buf);
    new_tcb->iss = 100;
    new_tcb->snd_una = new_tcb->iss;
    new_tcb->snd_nxt = new_tcb->iss;
    new_tcb->rcv_wnd = BUF_SIZE - 1; // reserve 1 byte for flow control

    LOG_INFO("[utcp_connect] Sending SYN packet...");
    int SYN_dgram = send_dgram(new_tcb, udp_fd, NULL, 0, TH_SYN);

    update_fsm(utcp_fd, SYN_SENT);

    while (new_tcb->fsm_state != ESTABLISHED) // block and wait for SYN-ACK
        pthread_cond_wait(&new_tcb->conn_cond, &new_tcb->lock);

    LOG_INFO("[utcp_connect] Unlocking the TCB...");
    pthread_mutex_unlock(&new_tcb->lock);

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


    args->udp_fd = bind_UDP_sock(global->client_port);

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

    tcb_t *tcb = get_tcb(args->utcp_fd);

    uint8_t buf[BUF_SIZE];
    ssize_t total_received = 0;

    while(1)
    {
        ssize_t rcvsize = utcp_recv(args->utcp_fd, buf, sizeof(buf));
        LOG_DEBUG("RECEIVE SIZE: %zu", rcvsize);
        if (rcvsize < 0)
            err_sys("[Client, listen thread] Error receiving packet");
        else if (rcvsize > 0) 
        {
            size_t safe_len = (rcvsize < sizeof(buf)) ? rcvsize : sizeof(buf) - 1;
            buf[safe_len] = '\0'; 
            total_received += rcvsize;
            
            LOG_INFO("[Client App] Received %zd bytes: %s", rcvsize, buf);
        }
    }

    free(args); // unreachable
    return 0;
}
