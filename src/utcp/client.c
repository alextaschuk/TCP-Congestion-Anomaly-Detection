/*
To run the client: `gcc -Iinclude client/client.c -o client/client && ./client/client`
*/
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
//#include <netinet/tcp.h>

#include <utcp/client.h>
#include <utcp/api.h>

#include <tcp/hndshk_fsm.h>

#include <utils/printable.h>
#include <utils/err.h>

int client_fsm = CLOSED; // TCP finite state machine for connection
int sock; // UDP socket
int UTCP_sock;
struct sockaddr_in client_addr, server_addr;

int main(void) {
    //set_server_port();
    //sock = bind_UDP_sock(&client_port);
    sock = bind_UDP_sock(5555);
    
    struct sockaddr_in client = {
    .sin_family = AF_INET,
    .sin_port = htons(client_utcp_port),
    .sin_addr.s_addr = inet_addr("127.0.0.1"),
    };
    UTCP_sock = bind_utcp(&client);

    struct sockaddr_in server = {
    .sin_family = AF_INET,
    .sin_port = htons(server_utcp_port),
    .sin_addr.s_addr = inet_addr("127.0.0.1"),
    };
    connect_utcp(UTCP_sock, &server, 4567);
    perform_hndshk(sock, UTCP_sock);
 
    //close(sock);
    return 0;
}

static void perform_hndshk(int sock, int utcp_fd)
{
    /**
     * @brief Initiate and carry out a 3-way handshake
     */
    // Create and configure TCP header for persistent connection
    struct tcb *tcb = get_tcb(utcp_fd);
    uint8_t *buffer = malloc(sizeof(tcphdr));
    struct sockaddr_in from;

    // Send the SYN datagram
    send_dgram(sock, utcp_fd, &buffer, 0, TH_SYN);

    tcb->fsm_state = SYN_SENT;
    tcb->snd_nxt += 1;
    printf("[Client]SYN packet sent\n");

    // Receive the SYN-ACK datagram
    uint8_t rcvbuf[1024];
    ssize_t rcvsize;
    rcvsize = rcv_dgram(sock, rcvbuf, &from);
    tcphdr *hdr;
    uint8_t* data;
    ssize_t data_len;
    deserialize_tcp_hdr(rcvbuf, rcvsize, &hdr, &data, &data_len);

    if (!(hdr->th_flags == (TH_SYN | TH_ACK)))
        err_sys("[perform_hndshk]Received datagram w/out SYN-ACK");
    printf("[Client]Received SYN-ACK\n"); 

    tcb->rcv_nxt = ntohl(hdr->th_seq) + 1;

    send_dgram(sock, utcp_fd, buffer, 0, TH_ACK);
    tcb->rcv_nxt = ntohl(hdr->th_seq) + 1;

    printf("[Client]ACK packet sent\n");
    update_fsm(utcp_fd, ESTABLISHED);
    
    free(buffer);
}

static void set_server_port()
{
    /**
     * @brief allows us to enter the server's port
     * number if we don't hardcode the value.
     */
    printf("enter the server's UDP port number:\r\n");
    scanf("%hd", &server_port);
}
