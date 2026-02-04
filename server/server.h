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

#define MAX_CONNECTIONS 1024 // allow 1024 connections at a time

uint8_t header[8]; //datagram header
uint8_t data[1028]; //buffer to send data
//char buffer[1024]; // buffer to receive data
int sock; // socket file descriptor

void begin_listen(int sock, int utcp_fd);
int handle_syn(struct tcphdr* hdr, int utcp_fd, struct sockaddr_in from);
int handle_ack(struct tcphdr* hdr, int utcp_fd, struct sockaddr_in from);
void send_syn_ack(uint8_t* buf);
