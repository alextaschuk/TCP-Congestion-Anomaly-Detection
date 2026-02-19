#include <utcp/server.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <utcp/api/api.h>
#include <utcp/api/conn.h>
#include <utcp/api/data.h>
#include <utcp/api/globals.h>
#include <utcp/api/ring_buffer.h>

#include <utils/printable.h>
#include <utils/err.h>

#include <tcp/hndshk_fsm.h>
#include <tcp/tcb.h>

//tcb_t *SYN_queue;
//tcb_t *accept_queue;

int main(void) {
    api_t *global = api_instance();
    //SYN_queue = calloc(SYN_BACKLOG, sizeof(tcb_t));
    //accept_queue = calloc(ACCEPT_BACKLOG, sizeof(tcb_t));

    struct sockaddr_in server = {
    .sin_family = AF_INET,
    .sin_port = htons(global->server_utcp_port),
    .sin_addr.s_addr = inet_addr("127.0.0.1"),
    };

    int UDP_sock = bind_UDP_sock(global->server_port);
    int UTCP_fd = bind_UTCP_sock(&server);

    begin_listen(UDP_sock, UTCP_fd);
}


void begin_listen(int sock, int utcp_fd)
{
    printf("[Server] Begin listen\n");
    update_fsm(utcp_fd, LISTEN);

    tcb_t *tcb = get_tcb(utcp_fd);

    ring_buf_init(&tcb->rx_buf, BUF_SIZE);
    ring_buf_init(&tcb->tx_buf, BUF_SIZE);
    uint8_t rcvbuf[BUF_SIZE]; // TODO replace with SYN queue and accept queue

    struct tcphdr *hdr; // TCP header in datagram's payload
    uint8_t* data; // data that comes after TCP header in payload
    ssize_t data_len; // num bytes of data
    ssize_t rcvsize; // num bytes of payload (TCP header + data)
    struct sockaddr_in from; // store info on who sent the datagram

    while (1) {
        rcvsize = rcv_dgram(sock, tcb, BUF_SIZE);
        deserialize_utcp_packet(rcvbuf, rcvsize, &hdr, &data, &data_len);

        printf("Sending a response to the test packet of data...\n");
        char *words = "Hi, this is my response\n";
        size_t len = strlen(words);
        send_dgram(sock, tcb, &words, len, 0);
        tcb->snd_una +=len;
        
        print_tcb(tcb);
    }
}
