#ifndef DATA_H
#define DATA_H

#include <stdint.h>
#include <sys/types.h>
#include <netinet/in.h>


uint8_t* build_datagram(uint16_t dest_port, char* dest_ip, uint8_t* buffer); // TODO
int send_dgram(int sock, int utcp_fd, void* buf, size_t len, int flags);
ssize_t rcv_dgram(int sock, uint8_t rcvbuf[1024], struct sockaddr_in* from);

#endif