#include <utcp/client.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/tcp.h>

#include <utcp/api/globals.h>
#include <utcp/api/api.h>
#include <utcp/api/conn.h>
#include <utcp/api/data.h>
#include <utcp/api/ring_buffer.h>

#include <tcp/hndshk_fsm.h>

#include <utils/printable.h>
#include <utils/err.h>

int main(void) {
    api_t *global = api_instance();
    //set_server_port();
    //sock = bind_UDP_sock(&client_port);
    
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

    int UDP_sock = bind_UDP_sock(global->client_port);
    int UTCP_sock = bind_UTCP_sock(&client);
    connect_utcp(UTCP_sock, &server, 4567);

    perform_hndshk(UDP_sock, UTCP_sock);
    return 0;
}


static void perform_hndshk(const int sock, const int utcp_fd)
{
    // Create and configure TCP header for persistent connection
    tcb_t *tcb = get_tcb(utcp_fd);
    ring_buf_init(&tcb->rx_buf, BUF_SIZE);
    ring_buf_init(&tcb->tx_buf, BUF_SIZE);
    uint8_t *buffer = malloc(sizeof(struct tcphdr));

    // 3WHS
    int SYN_dgram = send_dgram(sock, tcb, NULL, 0, TH_SYN);
    update_fsm(utcp_fd, SYN_SENT);
    ssize_t ACK_dgram = rcv_dgram(sock, tcb, BUF_SIZE);

    printf("Sending test packet of data...\n");
    char *words = "This is a test string";
    size_t len = strlen(words);
    send_dgram(sock, tcb, &words, len, 0);

    free(buffer);
    print_tcb(tcb);
}


static void set_server_port(void)
{
    api_t *global = api_instance();
    printf("enter the server's UDP port number:\r\n");
    scanf("%hd", &global->server_port);
}