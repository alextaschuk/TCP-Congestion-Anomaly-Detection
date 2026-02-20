#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

/* Define macros*/
#define MAX_CONNECTIONS 1024 // allow 1024 connections at a time
#define SYN_BACKLOG 128 // max number of connections in SYN queue during 3WHS
#define ACCEPT_BACKLOG 20 // max number of connections in incoming queue for `accept()`
/*End define macros*/

/* Define variables*/
extern uint8_t header[8]; //datagram header
extern uint8_t data[]; //buffer to send data
/* End define variables*/

/*Begin function declarations*/
/**
 * A background thread that will continuously listen
 * for incoming SYN requests. When a valid request
 * comes in, a new TCB is made (if possible).
 */
void* begin_listen(void *arg);
/*End function declarations*/

#endif
