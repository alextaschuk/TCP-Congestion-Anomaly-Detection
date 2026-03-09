#include <app/server_application.h>

#include <stdlib.h>

#include <arpa/inet.h>
#include <unistd.h>

#include <utils/err.h>
#include <utils/logger.h>
#include <utcp/api/conn.h>
#include <utcp/api/data.h>
#include <utcp/api/globals.h>
#include <utcp/server.h>

#include <zlog.h>


int app(void)
{

    uint16_t udp_port = 4567;
    int utcp_port = 332;

    if (init_zlog("zlog_server.conf") != 0) // initialize logger
        err_sys("Error initializing zlog");

    /* Bind sockets */
    struct sockaddr_in server = {
    .sin_family = AF_INET,
    .sin_port = htons(utcp_port),
    .sin_addr.s_addr = inet_addr("127.0.0.1"),
    };

    socket_fds *listen_socks = malloc(sizeof(socket_fds));
    if(!listen_socks)
        err_sys("[Client App] Failed to allocate socket_fds struct");

    listen_socks->udp_fd = bind_udp_sock(udp_port);
    listen_socks->utcp_fd = bind_utcp_sock(&server);
    
    LOG_INFO("[Server App] UDP & UTCP Sockets Initialized. UDP FD: %u,  Listen UTCP FD: %u", listen_socks->udp_fd, listen_socks->utcp_fd);

    /* Start listening and accept new connection requests */
    if (utcp_listen(listen_socks->utcp_fd, MAX_BACKLOG) != 0)
        err_sys("[Client App] Something went wrong in utcp_listen. Unable to listen for incoming connections");

    struct sockaddr_in client_info; // a peer to send/rcv data to/from
    int new_utcp_fd = utcp_accept(listen_socks, &client_info);
    
    /* Send data to the client */
    LOG_INFO("[Client App] opening tgg.txt");

    //FILE *fp = fopen("/Users/alex/Desktop/directed-study/jungle_book.txt", "rb"); // rb to prevent OS from changing newline characters
    FILE *fp = fopen("/Users/alex/Desktop/directed-study/tgg.txt", "rb"); // rb to prevent OS from changing newline characters
    if (!fp)
        err_sys("[Server App] Failed to open text file");
            
        uint8_t file_buf[2048];
        size_t bytes_read;
        size_t total_file_bytes = 0;

        while ((bytes_read = fread(file_buf, 1, sizeof(file_buf), fp)) > 0) 
            {
                size_t total_sent_this_chunk = 0;

                while (total_sent_this_chunk < bytes_read) 
                {
                    ssize_t sent = utcp_send(
                        new_utcp_fd,
                        listen_socks->udp_fd,
                        file_buf + total_sent_this_chunk,
                        bytes_read - total_sent_this_chunk
                    );

                    if (sent < 0)
                    {
                        LOG_ERROR("[Server App] Connection dropped during file transfer.");
                        break;
                    }
                    total_sent_this_chunk += sent;
                    total_file_bytes += sent;
                }
        }
        fclose(fp);
        LOG_INFO("[Server App] File queued successfully. Total bytes: %zu", total_file_bytes);
    
    free(listen_socks);
    sleep(2);
    return 0;
}