#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

/* Define variables*/
#define MAX_CONNECTIONS 1024 // allow 1024 connections at a time
#define SYN_BACKLOG 128 // max number of connections in SYN queue during 3WHS
#define ACCEPT_BACKLOG 20 // max number of connections in incoming queue for `accept()`

extern uint8_t header[8]; //datagram header
extern uint8_t data[]; //buffer to send data
//char buffer[1024]; // buffer to receive data
extern int sock; // socket file descriptor
/* End define variables*/

/*Begin function declarations*/

/**
 * @brief the provided socket is used to 
 * being listening for incoming datagrams.
 * 
 * @param sock A socket file descriptor that 
 * will receive incoming datagrams.
 */
void begin_listen(int sock, int utcp_fd);

/**
 * @brief called when a datagram has the `SYN` flag up. This updates
 * the TCB with the sender's UTCP port, UDP port, and IP address, the
 * initial recieve sequence #, the next expected sequence #, and finally
 * sends a `SYN-ACK` datagram in response.
 */
static void rcv_syn(int utcp_fd, const struct tcphdr *hdr, uint8_t *data, struct sockaddr_in from);

/**
 * @brief send a SYN-ACK in response to a
 * received SYN packet.
 */
static void send_syn_ack(int sock, int utcp_fd);

/**
 * @brief receive an ACK packet, and complete the 3-way handshake.
 */
void handle_ack(struct tcphdr* hdr, int utcp_fd, struct sockaddr_in from);

/*End function declarations*/

#endif
