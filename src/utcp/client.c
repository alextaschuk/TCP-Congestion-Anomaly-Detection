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

int client_fsm;
int sock;
int UTCP_sock;
tcb_t *tcb;
uint8_t *hndshk_buf; // empty, only for 
struct sockaddr_in client_addr;
struct sockaddr_in server_addr;

int main(void) {
    api_t *global = api_instance();
    int client_fsm = CLOSED; // TCP finite state machine for connection
    //set_server_port();
    //sock = bind_UDP_sock(&client_port);
    sock = bind_UDP_sock(5555);
    
    struct sockaddr_in client = {
    .sin_family = AF_INET,
    .sin_port = htons(global->client_utcp_port),
    .sin_addr.s_addr = inet_addr("127.0.0.1"),
    };
    UTCP_sock = bind_UTCP_sock(&client);
    tcb = get_tcb(UTCP_sock);
    tcb->fsm_state = CLOSED;


    struct sockaddr_in server = {
    .sin_family = AF_INET,
    .sin_port = htons(global->server_utcp_port),
    .sin_addr.s_addr = inet_addr("127.0.0.1"),
    };
    connect_utcp(UTCP_sock, &server, 4567);
    perform_hndshk(sock, UTCP_sock);
 
    //close(sock);
    return 0;
}

static void perform_hndshk(const int sock, const int utcp_fd)
{
    // Create and configure TCP header for persistent connection
    tcb_t *tcb = get_tcb(utcp_fd);
    ring_buf_init(&tcb->rx_buf, BUF_SIZE);
    ring_buf_init(&tcb->tx_buf, BUF_SIZE);
    uint8_t *buffer = malloc(sizeof(struct tcphdr));

    send_syn(sock, utcp_fd);
    rcv_syn_ack(sock, utcp_fd);    
    send_ack(sock, utcp_fd);
    
    
    printf("Client's current TCB:\n");
    print_tcb(tcb);

    //printf("Sending test packet of data...\n");
    //char *words = "This is";
    //size_t len = strlen(words);
    //send_dgram(sock, utcp_fd, &words, len, 0);
    //tcb->snd_una +=len;
    //char *words2 = " a test string";
    //len = strlen(words);
    //send_dgram(sock, utcp_fd, &words2, len, 0);
    //tcb->snd_una +=len;


    free(buffer);
}

static void send_syn(int sock, int utcp_fd)
{
    send_dgram(sock, utcp_fd, NULL, 0, TH_SYN);
    
    update_fsm(utcp_fd, SYN_SENT);
    tcb->snd_nxt += 1;

    printf("[Client]SYN packet sent\n");
    return;
}

static void rcv_syn_ack(int sock, int utcp_fd)
{
    // receive the SYN-ACK datagram
    uint8_t rcvbuf[BUF_SIZE]; // UDP datagram written into this (contains TCP header)
    ssize_t rcvsize; // number of bytes received
    struct sockaddr_in from; // info about who sent the SYN-ACK
    rcvsize = rcv_dgram(sock, rcvbuf, 1024, &from);

    // break the TCP header out of the payload and deserialize it
    struct tcphdr *hdr;
    uint8_t* data; // should be empty
    ssize_t data_len; // should be 0
    deserialize_tcp_hdr(rcvbuf, rcvsize, &hdr, &data, &data_len);

    if (data_len > 0) // we won't be using TCP Fast Open
        err_data("[rcv_syn_ack] ACK contains non-header data");

    if (!(hdr->th_flags == (TH_SYN | TH_ACK)))
        err_data("[perform_hndshk] Received datagram w/out SYN-ACK flags");

    tcb->rcv_nxt = ntohl(hdr->th_seq) + 1;

    printf("[Client]Received SYN-ACK\n"); 
    return;
}


static void send_ack(int sock, int utcp_fd)
{
    // for now, we won't let the app add data to the final ACK
    send_dgram(sock, utcp_fd, NULL, 0, TH_ACK);

    update_fsm(utcp_fd, ESTABLISHED);
    
    printf("[Client]ACK packet sent. Connection with server established.\n");
    return;
}


static void set_server_port(void)
{
    api_t *global = api_instance();
    printf("enter the server's UDP port number:\r\n");
    scanf("%hd", &global->server_port);
}
