#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/tcp.h>

#include "client.h"
#include "../api/api.h"
#include "../api/api.c"
#include "../pdp/hndshk_fsm.h"
#include "../pdp/pdp.h"

int client_fsm = CLOSED; // TCP finite state machine for connection
int sock; // socket file descriptor
struct sockaddr_in client_addr, server_addr;

int main(void) {
    // client socket
    sock = get_sock();
    memset(&client_addr, 0, sizeof(client_addr));
    get_sock_addr(&client_addr, client_ip, client_port);

    if(bind(sock, (struct sockaddr*)&client_addr, sizeof(client_addr)) == -1)
        err_sock(sock, "(main)failed to bind client socket");
    else
        printf("(main)client socket binding successful\n");

    // (server) listen socket
    memset(&server_addr, 0, sizeof(server_addr));
    get_sock_addr(&server_addr, server_ip, server_port);


    char buffer[sizeof()]; // define buffer for data to send
    send_syn(buffer);
 
    //close(sock);
    return 0;
}

void send_syn(char* buf)
{
    /**
     * @brief Send a SYN datagram to initiate 3-way handshake
     */
    // Create and configure TCP header for persistent connection
    struct tcphdr tcp_hdr;
    memset(&tcp_hdr, 0, sizeof(tcp_hdr));
    
    // set appropriate data and flags
    tcp_hdr.th_sport = htons(4567);
    tcp_hdr.th_dport = htons(7654); // should be server's listen port
    tcp_hdr.th_flags = TH_SYN; //set SYN flag to 1
    tcp_hdr.th_seq   = htonl(0x0000000);
    tcp_hdr.th_ack = htonl(0x0000000);
    
    // initialize PDP header
    Pdp_header client_pdp = make_hedr(tcp_hdr.th_seq, tcp_hdr.th_ack);

    // add TCP header and PDP header to datagram payload
    memcpy(buf, &tcp_hdr, sizeof(tcp_hdr));
    memcpy(buf + sizeof(tcp_hdr), &client_pdp, sizeof(client_pdp));

    // send the SYN datagram
    size_t dgram_len = sizeof(struct tcphdr) + sizeof(Pdp_header);
    int bytes_sent = sendto(sock, buf, dgram_len, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (bytes_sent < 0)
        err_sock(sock, "(send_syn)error sending packet");
    else 
    {
        client_fsm = SYN_SENT;
        fprintf(stdout, "SYN successfully sent\n");
    }
}

Pdp_header make_hedr(uint32_t seq, uint32_t ack)
{
    /**
     * @brief initialize a PDP header for a new connection
     * to be established with using all available info to
     * the client.
     */
    Pdp_header header;

    header.fourtuple.source_port = 4567;
    header.fourtuple.source_ip = "127.0.0.1";
    header.fourtuple.dest_ip = "127.0.0.1";
 // header.fourtuple.dest_port is set after SYN-ACK is rcv'd
    header.seq = seq;
    header.ack = ack;
    header.listen_port = server_port;
    header.fsm_state = client_fsm;

    return header;
}
