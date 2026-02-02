#ifndef API_H
#define API_H

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/*Begin function declarations*/
int get_sock();
int bind_sock();
int tcp_listen();
int tcp_connect();
int tcp_accept();

char* build_datagram(uint16_t dest_port, char* dest_ip, char* buffer);

char* rcv_datagram();
void send_datagram();
void get_sock_addr(struct sockaddr_in* addr, const char* ip_addr, uint16_t port);
uint32_t calc_seq();

static void err_sys(const char* x);
void err_sock(int sock, const char* x);
/*End function declarations*/




#endif