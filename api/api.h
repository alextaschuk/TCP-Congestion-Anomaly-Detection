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

#include <hndshk_fsm.h>
#include <tcb.h>
#include <fourtuple.h>

#define MAX_UTCP_SOCKETS 1024

/*Begin function declarations*/
uint8_t* build_datagram(uint16_t dest_port, char* dest_ip, uint8_t* buffer);
ssize_t rcv_dgram(int sock, uint8_t rcvbuf[1024], struct sockaddr_in* from);

int send_dgram(int sock, int utcp_fd, void* buf, size_t len, int flags);
//int bind_UDP_sock(uint16_t* pts);
int bind_UDP_sock(int pts);
int bind_utcp(struct sockaddr_in *addr);

void err_sock(int sock, const char* x);
void connect_utcp(int utcp_fd, struct sockaddr_in* addr, uint16_t dest_udp);
void deserialize_tcp_hdr(uint8_t* buf, size_t buflen, struct tcphdr **out_hdr, uint8_t **out_data, ssize_t *out_data_len);
void update_fsm(int utcp_fd, enum conn_state state);

static void err_sys(const char* x);
struct tcb* get_tcb(int utcp_fd);
/*End function declarations*/
#endif