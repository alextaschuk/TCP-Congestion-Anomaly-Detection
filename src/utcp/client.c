#include <utcp/client.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/tcp.h>

#include <utcp/api/api.h>
#include <utcp/api/conn.h>
#include <utcp/api/data.h>
#include <utcp/api/rx_dgram.h>
#include <utcp/api/tx_dgram.h>
#include <utcp/api/ring_buffer.h>

#include <tcp/hndshk_fsm.h>

#include <utils/printable.h>
#include <utils/err.h>

int main(void) {
    api_t *global = api_instance();
    listen_args_t *args = malloc(sizeof(listen_args_t));

    init_client(args, global);
    init_hndshk(args);

    // spin up a listener thread
    pthread_t listen_thread;
    if (pthread_create(&listen_thread, NULL, begin_listen, args) != 0)
        err_sys("[Client, main] Failed to create listener thread");

    tcb_t *tcb = get_tcb(args->utcp_fd);
    while(1)
    {
        sleep(2);
        if(tcb->fsm_state == ESTABLISHED)
        {
        printf("Sending a test packet of data...\n");
        
        char *words = "Test data from client\n";
        size_t len = strlen(words);
        size_t written = utcp_send(tcb, words, len);

        // client determines how much it can send
        uint32_t flight = tcb->snd_nxt - tcb->snd_una; // bytes in flight
        printf("[utcp_recv] rcv_wnd update: %u\n", tcb->rcv_wnd);
        uint32_t wnd = tcb->rcv_wnd;
        uint32_t available = ring_buf_used(&tcb->tx_buf) - flight;
        uint32_t to_send = MIN(available, wnd);

        uint8_t tmp[MSS];
        len = MIN(MSS, available);

        ring_buf_peek(&tcb->tx_buf, tmp, len);

        send_dgram(args->udp_fd, tcb, tmp, len, 0);
        printf("[client, main thread] test packet sent\n");
        
        sleep(10000);
        }
    }
    free(args);
    return 0;
}


static void init_hndshk(void *arg)
{
    // Create and configure TCP header
    listen_args_t *args = (listen_args_t*)arg;
    tcb_t *tcb = get_tcb(args->utcp_fd);
    ring_buf_init(&tcb->rx_buf);
    ring_buf_init(&tcb->tx_buf);
    uint8_t *buffer = malloc(sizeof(struct tcphdr));

    // 3WHS
    int SYN_dgram = send_dgram(args->udp_fd, tcb, NULL, 0, TH_SYN);
    update_fsm(args->utcp_fd, SYN_SENT);
    ssize_t ACK_dgram = rcv_dgram(args->udp_fd, BUF_SIZE);

    free(buffer);
    print_tcb(tcb);
}


void* begin_listen(void *arg)
{
    printf("[Client] Begin listen\n");

    listen_args_t *args = (listen_args_t*)arg;
    tcb_t         *tcb = get_tcb(args->utcp_fd);
    uint8_t       rcvbuf[BUF_SIZE]; // TODO replace with SYN queue and accept queue

    struct tcphdr      *hdr;     // TCP header in datagram's payload
    struct sockaddr_in from;     // store info on who sent the datagram
    uint8_t*           data;     // data that comes after TCP header in payload
    ssize_t            data_len; // num bytes of data
    ssize_t            rcvsize;  // num bytes of payload (TCP header + data)

    while (1) {
        if(tcb->fsm_state == ESTABLISHED)
        {
            printf("\n[client, listen thread]: Received a packet\n\n");
            rcvsize = rcv_dgram(args->udp_fd, BUF_SIZE);
            print_tcb(tcb);
        }
    }
}


static void set_server_port(void)
{
    api_t *global = api_instance();
    printf("enter the server's UDP port number:\r\n");
    scanf("%hd", &global->server_port);
}


static int init_client(void *arg, api_t *global)
{
    listen_args_t *args = (listen_args_t*)arg;

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
