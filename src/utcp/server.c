#include <utcp/server.h>

#include <errno.h>
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
#include <utcp/api/globals.h>
#include <utcp/api/ring_buffer.h>

#include <utils/err.h>
#include <utils/printable.h>

#include <tcp/hndshk_fsm.h>
#include <tcp/tcb.h>

//https://github.com/MichaelDipperstein/sockets/blob/master/echoserver_udp.c
//https://csperkins.org/teaching/2007-2008/networked-systems/lecture04.pdf
//https://github.com/goToMain/c-utils


//https://www.alibabacloud.com/blog/tcp-syn-queue-and-accept-queue-overflow-explained_599203
//https://blog.cloudflare.com/syn-packet-handling-in-the-wild/
// https://chatgpt.com/c/6989333e-0634-8333-ad74-64d879516a25 -- for rcv wnd stuff
// https://cabulous.medium.com/tcp-send-window-receive-window-and-how-it-works-8629a4fad9ec
// https://docs.kernel.org/trace/ring-buffer-design.html -- lockless ring buffer

//tcb_t *SYN_queue;
//tcb_t *accept_queue;


int sock; // udp socket

int main(void) {
    api_t *global = api_instance();
    int client_fsm = CLOSED;
    //SYN_queue = calloc(SYN_BACKLOG, sizeof(tcb_t));
    //accept_queue = calloc(ACCEPT_BACKLOG, sizeof(tcb_t));

    //if (server_port == 0)
    sock = bind_UDP_sock(4567);

    struct sockaddr_in server = {
    .sin_family = AF_INET,
    .sin_port = htons(global->server_utcp_port),
    .sin_addr.s_addr = inet_addr("127.0.0.1"),
    };

    int UTCP_sock = bind_UTCP_sock(&server);

    begin_listen(sock, UTCP_sock);
}

void begin_listen(int sock, int utcp_fd)
{
    printf("Begin listen\n");
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
        if (ring_buf_free(&tcb->rx_buf) == 0)
            return; // buffer is full, ignore incoming data

        rcvsize = rcv_dgram(sock, rcvbuf, 1024, &from); 
        deserialize_tcp_hdr(rcvbuf, rcvsize, &hdr, &data, &data_len);

        // Check if SYN flag is set (will this overwrite other flags?)
        if(tcb->fsm_state == LISTEN && hdr->th_flags & TH_SYN)
        {
            if (data_len > 0)
                err_data("[begin_listen](SYN) SYN contains non-header data");

            if(hdr->th_dport != tcb->fourtuple.source_port)
                err_sys("[begin_listen]header dest port doesn't match tcb src port");

            rcv_syn(utcp_fd, hdr, data, from);
            send_syn_ack(sock, utcp_fd);

        }

        if(tcb->fsm_state == SYN_RECEIVED && hdr->th_flags & TH_ACK)
        {
            if (data_len > 0)
                err_data("[begin_listen](ACK) SYN contains non-header data");
            
                handle_ack(hdr, utcp_fd, from);
        }

        if(tcb->fsm_state == ESTABLISHED)
        {
            uint32_t seq = hdr->th_seq;

            if(seq != tcb->rcv_nxt)
                return; //ignore out of order data for now
            
            size_t free = ring_buf_free(&tcb->rx_buf);
            if ((size_t)data_len > free)
                return; // can't fit all data in the buffer, ignore (for now)

            ring_buf_write(&tcb->rx_buf, data, data_len);
            tcb->rcv_nxt += data_len;
            tcb->rcv_wnd = ring_buf_free(&tcb->rx_buf);
            send_dgram(sock, utcp_fd, NULL, 0, TH_ACK); // ACK the received data
            print_tcb(tcb);
        }
    }
}


static void rcv_syn(int utcp_fd, const struct tcphdr *hdr, uint8_t *data, struct sockaddr_in from)
{
    tcb_t *tcb = get_tcb(utcp_fd);

    // initialize TCB (TODO: move to separate function?)
    update_fsm(utcp_fd, SYN_RECEIVED);
    tcb->fourtuple.dest_port = hdr->th_sport;
    tcb->fourtuple.dest_ip = ntohl(from.sin_addr.s_addr);
    tcb->dest_udp_port = ntohs(from.sin_port);
    tcb->iss = 0x0000; // initial seq # = 0

    tcb->irs = ntohs(hdr->th_seq);
    tcb->rcv_nxt = tcb->irs + 1; // Increase sequence number by one
    tcb->rcv_wnd = BUF_SIZE;

    printf("[Server] Received a valid SYN packet.\n");
    return;
}

static void send_syn_ack(int sock, int utcp_fd)
{
    send_dgram(sock, utcp_fd, NULL, 0, TH_SYN | TH_ACK);
    
    printf("[Server] Sent SYN-ACK\n");
    return;
}


void handle_ack(struct tcphdr* hdr, int utcp_fd, struct sockaddr_in from)
{
    printf("[Server] Received ACK\n");

    update_fsm(utcp_fd, ESTABLISHED);
    tcb_t *tcb = get_tcb(utcp_fd);
    
    printf("Server's current TCB:\n");
    print_tcb(tcb);
}
