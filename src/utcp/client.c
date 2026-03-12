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


static int utcp_connect(int utcp_fd, const struct sockaddr_in *dest_addr)
{
    api_t *global = api_instance();

    LOG_INFO("[utcp_connect] Creating a new TCB for the application's connection request...");

    tcb_t *new_tcb = get_tcb(utcp_fd);

    pthread_mutex_lock(&new_tcb->lock);
    //LOG_INFO("[utcp_connect] Locked the TCB...");
    new_tcb->fourtuple.dest_ip     = ntohl(dest_addr->sin_addr.s_addr);
    new_tcb->fourtuple.dest_port   = ntohs(dest_addr->sin_port);
    
    new_tcb->dest_udp_port         = global->server_udp_port;
    new_tcb->src_udp_port          = global->udp_port; // assigned by the OS

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


int main(void) {
    if (init_zlog("zlog_client.conf") != 0) // initialize logger
        err_sys("Error initializing zlog");

    api_t *global = api_instance();

    bind_udp_sock(0);
    int utcp_fd = init_utcp_sock();

    struct sockaddr_in client = {
        .sin_family = AF_INET, 
        .sin_port = htons(8292), 
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    struct sockaddr_in server = {
        .sin_family = AF_INET, 
        .sin_port = htons(332), 
        .sin_addr.s_addr = inet_addr("40.82.162.155")
    };

    bind_utcp_sock(utcp_fd, &client);
    utcp_connect(utcp_fd, &server);

    /* pretend to be a client app */
    FILE *fp = fopen("../test_rcvd.txt", "wb"); // wb to ensure it's an exact copy
    if (!fp) {
        err_sys("[Client App] Failed to create destination file");
    }
    //size_t file_size_bytes = 1000000000; // 1GB
    size_t file_size_bytes = 10000000; // 10mb
    uint8_t *app_rcv_buf = malloc(BUF_SIZE + 1);
    size_t total_received = 0;

    while(total_received < file_size_bytes)
    {   
        ssize_t bytes_rcvd = utcp_recv(utcp_fd, app_rcv_buf, BUF_SIZE);
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

    LOG_INFO("[Client App] Finished. Received %zu bytes total", total_received);
    fclose(fp);
    free(app_rcv_buf);
    return 0;
}
