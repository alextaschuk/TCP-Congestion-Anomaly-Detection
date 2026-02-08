/*
To run the server: `gcc -Iinclude server/server.c -o server/server && ./server/server`
*/
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

#include <utcp/server.h>
#include <utcp/api.h>

#include <tcp/hndshk_fsm.h>
#include <tcp/tcb.h>

//https://github.com/MichaelDipperstein/sockets/blob/master/echoserver_udp.c
//https://csperkins.org/teaching/2007-2008/networked-systems/lecture04.pdf

extern uint8_t header[8]; //datagram header
extern uint8_t data[1028]; //buffer to send data
int sock;

int main(void) {
    //if (server_port == 0)
    sock = bind_UDP_sock(4567);

    struct sockaddr_in server = {
    .sin_family = AF_INET,
    .sin_port = htons(server_utcp_port),
    .sin_addr.s_addr = inet_addr("127.0.0.1"),
    };

    int UTCP_sock = bind_utcp(&server);

    begin_listen(sock, UTCP_sock);
}

void begin_listen(int sock, int utcp_fd)
{
    /**
     * @brief the provided socket is used to 
     * being listening for incoming datagrams.

     * @param sock A socket file descriptor that 
     * will receive incoming datagrams.
     */
    printf("Begin listen\n");
    update_fsm(utcp_fd, LISTEN);
    // Needs to accomplish two things:
        // 1. Handle handshake packets (SYN, ACK)
        // 2. handle other data packets

    uint8_t rcvbuf[1024];
    ssize_t rcvsize;
    struct sockaddr_in from;
    while (1) {
        rcvsize = rcv_dgram(sock, rcvbuf, &from); 
        tcphdr *hdr;
        uint8_t* data;
        ssize_t data_len;
        deserialize_tcp_hdr(rcvbuf, rcvsize, &hdr, &data, &data_len);

        // Check if SYN flag is set (will this overwrite other flags?)
        if(hdr->th_flags & TH_SYN)
            handle_syn(hdr, utcp_fd, from);
        if(hdr->th_flags & TH_ACK)
            handle_ack(hdr, utcp_fd, from);

        //printf("recsize: %d\n", (int)rcvsize);
        //printf("datagram: %.*s\n", (int)rcvsize, rcvbuf);
    }
}



int handle_syn(tcphdr* hdr, int utcp_fd, struct sockaddr_in from)
{
    /**
     * @brief called when a datagram has the SYN flag up. This updates
     * the TCB with the sender's UTCP port, UDP port, and IP address, the
     * initial recieve sequence #, the next expected sequence #, and finally
     * sends a SYN-ACK datagram in response.
     */
    update_fsm(utcp_fd, SYN_RECEIVED);
    struct tcb *tcb = get_tcb(utcp_fd);
    
    if(hdr->th_dport != tcb->fourtuple.source_port)
        err_sys("[handle_syn]header dest port doesn't match tcb src port");

    tcb->fourtuple.dest_port = hdr->th_sport;
    tcb->fourtuple.dest_ip = ntohl(from.sin_addr.s_addr);
    tcb->dest_udp_port = ntohs(from.sin_port);
    tcb->irs = ntohs(hdr->th_seq);
    tcb->rcv_nxt = tcb->irs + 1; // Increase sequence number by one

    send_dgram(sock, utcp_fd, NULL, 0, TH_SYN | TH_ACK);
    return 1;
}

int handle_ack(tcphdr* hdr, int utcp_fd, struct sockaddr_in from)
{
    printf("[Server]Received ACK\n");
    update_fsm(utcp_fd, ESTABLISHED);
    printf("seq num: %u\n", hdr->th_seq);
    printf("ack num: %u\n", hdr->th_ack);
    return 1;
}

