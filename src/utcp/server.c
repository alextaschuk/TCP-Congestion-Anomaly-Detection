#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
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
#include <utcp/api/globals.h>
#include <utcp/rx/rx_dgram.h>
#include <utcp/api/tx_dgram.h>
#include <utcp/api/utcp_timers.h>

#include <zlog.h>


_Thread_local const char* current_thread_cat = "main_thread";

int main(void)
{
    ///*
    // Use this for receiving data from the client
    if (init_zlog("zlog_server.conf") != 0) // initialize logger
        err_sys("Error initializing zlog");

    api_t *global = api_instance();
    global->utcp_fd = 0; // listen socket will always have fd of 0
    int utcp_fd = utcp_sock();

    struct sockaddr_in server = {
        .sin_family = AF_INET, 
        .sin_port = htons(332), // UTCP port 
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    
    utcp_bind(utcp_fd, &server);

    if (utcp_listen(global, MAX_BACKLOG) != 0)
        err_sys("[main] Error in utcp_listen");
    
    tcb_t *listen_tcb = get_tcb(utcp_fd);

    pthread_mutex_lock(&listen_tcb->lock);
    listen_tcb->src_udp_fd = global->udp_fd;
    pthread_mutex_unlock(&listen_tcb->lock);

    int new_utcp_fd = utcp_accept(global);
    tcb_t *new_tcb = get_tcb(new_utcp_fd);

    pthread_mutex_lock(&new_tcb->lock);
    new_tcb->src_udp_fd = global->udp_fd;
    pthread_mutex_unlock(&new_tcb->lock);

    FILE *fp = fopen("../test_rcvd.txt", "wb"); // wb to ensure it's an exact copy
    if (!fp)
        err_sys("[Server App] Failed to create destination file");
    
    size_t file_size_bytes = 5000000000;// 5GB
    //uint8_t *app_recv_buf = malloc(APP_BUF_SIZE);
    uint8_t *app_recv_buf = malloc(65536);
    size_t total_recvd = 0;

    printf("Server: Begin receiving %zuGB file from server...\r\n", file_size_bytes / 5000000000);
    while(total_recvd < file_size_bytes)
    {   
        ssize_t bytes_rcvd = utcp_recv(new_tcb->fd, app_recv_buf, 65536);
        if (bytes_rcvd > 0)
        {
            fwrite(app_recv_buf, 1, bytes_rcvd, fp);
            total_recvd += (size_t)bytes_rcvd;
        }
        if (bytes_rcvd < 0)
        {
            LOG_ERROR("[Server App] Error receiving data.");
            break; 
        }
    }
    
    while(new_tcb->rx_tail - new_tcb->rx_head > 0)
        usleep(100000); // let everything in RX go to the application
    sleep(2);

    LOG_INFO("[Server App] Finished. Received %zu bytes total", total_recvd);
    fclose(fp);
    free(app_recv_buf);
    return 0;
    //*/

    /*
    // Use this for sending data to client
    if (init_zlog("zlog_server.conf") != 0) // initialize logger
        err_sys("Error initializing zlog");

    api_t *global = api_instance();
    global->utcp_fd = 0; // listen socket will always have fd of 0
    int utcp_fd = utcp_sock();

    struct sockaddr_in server = {
        .sin_family = AF_INET, 
        .sin_port = htons(332), // UTCP port 
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    
    utcp_bind(utcp_fd, &server);
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
            
    struct stat st;
    if (stat("../test_file.txt", &st) == -1)
        err_sys("[Client App] Failed to get filesize");
    
    size_t file_size_bytes = (size_t)st.st_size;
    //uint8_t *snd_buf = malloc(APP_BUF_SIZE);
    uint8_t *snd_buf = malloc(64000);
    size_t bytes_read = 0;
    size_t total_file_bytes = 0;

    printf("Server: Ready to send %zuGB file to client...\r\n", file_size_bytes / 1000000000);
    while((bytes_read = fread(snd_buf, 1, 64000, fp)) > 0)
    {
        ssize_t sent = utcp_send(new_utcp_fd, snd_buf, bytes_read);
        if (sent < 0)
        {
            LOG_ERROR("[Server App] Connection dropped during file transfer.");
            break;
        }

        total_file_bytes += sent;
        printf("Server Application: bytes sent: %zu/%zu\r", total_file_bytes, file_size_bytes);
        fflush(stdout);
    }

    tcb_t *active_tcb = get_tcb(utcp_fd); 
    while(active_tcb->tx_tail - active_tcb->tx_head > 0)
        usleep(100000); // let everything in TX be sent out
    sleep(2);

        fclose(fp);
        free(snd_buf);
        LOG_INFO("[Server App] File queued successfully. Total bytes: %zu", total_file_bytes);
        return 0;
    */
}
