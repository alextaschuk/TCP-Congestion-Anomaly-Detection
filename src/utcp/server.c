#include <utcp/server.h>

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
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
#include <utcp/api/rx_dgram.h>
#include <utcp/api/tx_dgram.h>
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

    listen_args_t *args = malloc(sizeof(listen_args_t));
    args->udp_sock = bind_UDP_sock(global->server_port);
    args->utcp_fd = bind_UTCP_sock(&server);
    pthread_t listen_thread;

    tcb_t *tcb = get_tcb(args->utcp_fd);
    printf("[Server, main] Spawning listener thread...\n");
    if (pthread_create(&listen_thread, NULL, begin_listen, args) != 0)
        err_sys("[Server, main] Failed to create listener thread");

    while(1) 
    {
        // main thread -- send data and do other stuff
        // maybe logic to update buffers when apps want data
        if(tcb->fsm_state == ESTABLISHED)
        {
        printf("Sending a response to the test packet of data...\n");
        char *words = "Hi, this is my response\n";
        size_t len = strlen(words);
        size_t written = utcp_send(tcb, words, len);

        // determine how much we can send
        uint32_t flight = tcb->snd_nxt - tcb->snd_una; // bytes in flight
        printf("[utcp_recv] rcv_wnd update: %u\n", tcb->rcv_wnd);
        uint32_t wnd = tcb->rcv_wnd;
        uint32_t available = ring_buf_used(&tcb->tx_buf) - flight;
        uint32_t to_send = MIN(available, wnd);

        uint8_t tmp[MSS];
        len = MIN(MSS, available);

        ring_buf_peek(&tcb->tx_buf, tmp, len);
        send_dgram(args->udp_sock, tcb, tmp, len, 0);
        print_tcb(tcb);
        sleep(10000);
        }

    }

    return 0;

}


void* begin_listen(void *arg)
{
    printf("[Server] Begin listen\n");

    listen_args_t *args = (listen_args_t*)arg;
    tcb_t         *tcb = get_tcb(args->utcp_fd);
    uint8_t       rcvbuf[BUF_SIZE]; // TODO replace with SYN queue and accept queue

    struct tcphdr      *hdr; // TCP header in datagram's payload
    struct sockaddr_in from; // store info on who sent the datagram
    uint8_t*           data; // data that comes after TCP header in payload
    ssize_t            data_len; // num bytes of data
    ssize_t            rcvsize; // num bytes of payload (TCP header + data)
    
    ring_buf_init(&tcb->rx_buf);
    ring_buf_init(&tcb->tx_buf);
    update_fsm(args->utcp_fd, LISTEN);

    while (1) {
        rcvsize = rcv_dgram(args->udp_sock, tcb, BUF_SIZE);
        deserialize_utcp_packet(rcvbuf, rcvsize, &hdr, &data, &data_len);
        print_tcb(tcb);
    }
}

