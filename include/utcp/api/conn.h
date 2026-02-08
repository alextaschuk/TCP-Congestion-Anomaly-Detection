#ifndef CONN_H
#define CONN_H

#include <stdint.h>
#include <netinet/in.h>

int bind_UDP_sock(int pts); // binds a port to a socket
int bind_UTCP_sock(struct sockaddr_in *addr); // binds a UTCP port to a UTCP socket
void connect_utcp(int utcp_fd, struct sockaddr_in* addr, uint16_t dest_udp); // connect a UTCP socket to a remote UTCP socket

#endif