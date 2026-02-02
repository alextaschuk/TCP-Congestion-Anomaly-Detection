#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "server.h"
#include "../api/api.h"
#include "../api/api.c"
#include "../pdp/hndshk_fsm.h"
#include "../pdp/pdp.h"

//https://github.com/MichaelDipperstein/sockets/blob/master/echoserver_udp.c
//https://csperkins.org/teaching/2007-2008/networked-systems/lecture04.pdf


struct sockaddr_in client_addr, server_addr;
int server_fsm = CLOSED; // TCP finite state machine for connection
Pdp_header server_pdp;
Pdp_header *lookup_table;

int main(void) {
    lookup_table = malloc(sizeof(Pdp_header) * MAX_CONNECTIONS);
    int sock = get_sock();

    memset(&server_addr, 0, sizeof(server_addr));
    get_sock_addr(&server_addr, server_ip, server_port);

    if(bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
        err_sock(sock, "failed to bind server socket");
    else
        printf("server socket binding successful\n");

    hndshk_lstn(sock); // begin listening
}

void hndshk_lstn(int sock)
{
    /**
     * @brief a listen function that to open a listening socket for performing
     * a 3-way handshake
     * @note hndshk_lstn -> Handshake Listen
     * @param sock A socket file descriptor that 
     * will receive incoming SYN messages
     */
    printf("Begin listen\n");
    struct sockaddr_storage from;
    char rcvbuf[1024];
    ssize_t rcvsize;
    while (1) {
        socklen_t fromlen = sizeof from;
        rcvsize = recvfrom(sock, rcvbuf, sizeof(rcvbuf), 0, (struct sockaddr *)&from, &fromlen); // # bytes rcv'd

        if (rcvsize < 0)
            err_sys("server failed to receive datagram");
        
        rcv_syn(rcvbuf);



        printf("recsize: %d\n", (int)rcvsize);
        printf("datagram: %.*s\n", (int)rcvsize, rcvbuf);
    }
}


//Pdp_header* find_connection(struct fourtuple fourtuple)
//{
//    /**
//     * @brief uses a four tuple to check the lookup table
//     * for an exisiting connection. 
//     * 
//     * @return pointer to element in lookup table if found,
//     * NULL otherwise.
//     */
//}


int rcv_syn(char* buf)
{
    /**
     * @brief receive a SYN datagram, parse it, and handle its contents.
     * 
     * Returns a new socket fd for the client's connection. bind() the socket
     * with the port # and IP, then connect() it to the client's socket using
     * the info provided in the PDP header.
     */
    struct tcphdr *tcp_hdr = (struct tcphdr *)(void *)buf; // cast to tcphdr struct
    printf("SEQ NUM: %u\n", ntohl(tcp_hdr->th_seq)); // convert fields from network byte order

    struct Pdp_header *syn_pdp = (struct Pdp_header *)(buf + sizeof(struct tcphdr));
    printf("FSM state: %d\n", syn_pdp->fsm_state);
    printf("PDP seq: %u\n", syn_pdp->seq);

}


