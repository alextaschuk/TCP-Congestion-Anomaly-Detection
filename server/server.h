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

#include "../pdp/pdp.h"

#define MAX_CONNECTIONS 1024 // allow 1024 connections at a time

char header[8]; //datagram header
char data[1028]; //buffer to send data
//char buffer[1024]; // buffer to receive data
int sock; // socket file descriptor

void hndshk_lstn(int sock);
Pdp_header* find_connection(struct fourtuple fourtuple);
int rcv_syn(char* buf);
void send_syn_ack(char* buf);
