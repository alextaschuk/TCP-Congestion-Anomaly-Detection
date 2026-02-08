#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

/* Define variables*/
#define MAX_CONNECTIONS 1024 // allow 1024 connections at a time

extern uint8_t header[8]; //datagram header
extern uint8_t data[1028]; //buffer to send data
//char buffer[1024]; // buffer to receive data
extern int sock; // socket file descriptor
/* End define variables*/

/*Begin function declarations*/
void begin_listen(int sock, int utcp_fd);
int handle_syn(struct tcphdr* hdr, int utcp_fd, struct sockaddr_in from);
int handle_ack(struct tcphdr* hdr, int utcp_fd, struct sockaddr_in from);
void send_syn_ack(uint8_t* buf);
/*End function declarations*/

#endif
