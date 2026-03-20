#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <tcp/hndshk_fsm.h>
#include <tcp/tcb.h>
#include <utils/err.h>
#include <utils/logger.h>
#include <utils/printable.h>
#include <utcp/api/api.h>
#include <utcp/api/conn.h>
#include <utcp/api/data.h>
#include <utcp/api/globals.h>
#include <utcp/rx/rx_dgram.h>
#include <utcp/api/tx_dgram.h>

#include <zlog.h>


_Thread_local const char* current_thread_cat = "main_thread";


int main(void)
{
    ///*
    // Use this for sending data to the server
    if (init_zlog("zlog_client.conf") != 0) // initialize logger
        err_sys("Error initializing zlog");

    bind_udp_sock(0);
    int utcp_fd = utcp_sock();

    struct sockaddr_in client =
    {
        .sin_family = AF_INET, 
        .sin_port = htons(8292), 
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    struct sockaddr_in server =
    {
        .sin_family = AF_INET, 
        .sin_port = htons(332), 
        .sin_addr.s_addr = inet_addr("40.82.162.155")
    };

    utcp_bind(utcp_fd, &client);
    utcp_connect(utcp_fd, &server);

    FILE *fp = fopen("../test_file.txt", "rb"); // rb to prevent OS from changing newline characters
    if (!fp)
        err_sys("[Client App] Failed to open text file");
            
    struct stat st;
    if (stat("../test_file.txt", &st) == -1)
        err_sys("[Client App] Failed to get filesize");
    
    size_t file_size_bytes = (size_t)st.st_size;
    //uint8_t *snd_buf = malloc(APP_BUF_SIZE);
    uint8_t *snd_buf = malloc(64000);
    size_t bytes_read = 0;
    size_t total_file_bytes = 0;

    printf("Client: Ready to send %zuGB file to client...\r\n", file_size_bytes / 1000000000);
    while((bytes_read = fread(snd_buf, 1, 64000, fp)) > 0)
    {
        ssize_t sent = utcp_send(utcp_fd, snd_buf, bytes_read);
        if (sent < 0)
        {
            LOG_ERROR("[Client App] Connection dropped during file transfer.");
            break;
        }

        total_file_bytes += sent;
        printf("Client Application: bytes sent: %zu/%zu\r", total_file_bytes, file_size_bytes);
        fflush(stdout);
    }

    tcb_t *active_tcb = get_tcb(utcp_fd); 
    while(active_tcb->tx_tail - active_tcb->tx_head > 0)
        usleep(100000); // let everything in TX be sent out
    sleep(2);

        fclose(fp);
        free(snd_buf);
        LOG_INFO("[Client App] File queued successfully. Total bytes: %zu", total_file_bytes);
        return 0;
    //*/

    /*
    // use this for receiving data from server
    if (init_zlog("zlog_client.conf") != 0) // initialize logger
        err_sys("Error initializing zlog");

    bind_udp_sock(0);
    int utcp_fd = utcp_sock();

    struct sockaddr_in client =
    {
        .sin_family = AF_INET, 
        .sin_port = htons(8292), 
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    struct sockaddr_in server =
    {
        .sin_family = AF_INET, 
        .sin_port = htons(332), 
        .sin_addr.s_addr = inet_addr("40.82.162.155")
    };

    utcp_bind(utcp_fd, &client);
    utcp_connect(utcp_fd, &server);

    FILE *fp = fopen("../test_rcvd.txt", "wb"); // wb to ensure it's an exact copy
    if (!fp)
        err_sys("[Client App] Failed to create destination file");

    size_t file_size_bytes = 1000000000;// 1GB
    //uint8_t *app_recv_buf = malloc(APP_BUF_SIZE);
    uint8_t *app_recv_buf = malloc(65536);
    size_t total_received = 0;

    printf("Client: Ready to receive %zuGB file from server...\r\n", file_size_bytes / 1000000000);
    while(total_received < file_size_bytes)
    {   
        ssize_t bytes_rcvd = utcp_recv(utcp_fd, app_recv_buf, APP_BUF_SIZE);
        if (bytes_rcvd > 0)
        {
            fwrite(app_recv_buf, 1, bytes_rcvd, fp);
            fflush(fp); // forces the OS to write to the txt file immediately
            total_received += (size_t)bytes_rcvd;
            printf("Client Application: Wrote %zd bytes to disk. Total: %zu/%zu\r", bytes_rcvd, total_received, file_size_bytes);
            fflush(stdout);
        }
        if (bytes_rcvd < 0)
        {
            LOG_ERROR("[Client App] Error receiving data.");
            break; 
        }
    }
    tcb_t *active_tcb = get_tcb(utcp_fd);
    while(active_tcb->rx_tail - active_tcb->rx_head > 0)
        usleep(100000); // let everything in RX go to the client
    sleep(10);

    LOG_INFO("[Client App] Finished. Received %zu bytes total", total_received);
    fclose(fp);
    free(app_recv_buf);
    return 0;
    // */
}
