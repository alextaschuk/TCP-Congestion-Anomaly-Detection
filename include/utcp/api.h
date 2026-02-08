#ifndef API_H
#define API_H

#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/tcp.h>


#include <tcp/hndshk_fsm.h>
#include <tcp/tcb.h>

/* Define variables*/
#define MAX_UTCP_SOCKETS 1024

extern uint16_t client_port;
extern const int client_utcp_port; 
extern const char* client_ip;
extern uint16_t server_port;
extern const int server_utcp_port;
extern const char* server_ip;
/* End define variables*/

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

void err_sys(const char* x);
struct tcb* get_tcb(int utcp_fd);
/*End function declarations*/
#endif
