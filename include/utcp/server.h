#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <utcp/api/globals.h>

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
 * @brief Initializes the server's UDP and UTCP sockets.
 * 
 * A UDP socket and UTCP socket are bound to the server.
 * `*arg` stores pointers to the socket file descriptors.
 * A TCB is made for the listen socket, and the bound UTCP
 * port and the server's IP address are stored in it.
 * @param *args A `listen_args_t` struct.
 * @param *global A pointer to the global api struct.
 */
static void init_server(listen_args_t *args, api_t *global);

/**
 * @brief Listens for incoming connection requests and handles
 * them accordingly.
 * 
 * This function runs on a background thread that will continuously
 * listen for incoming SYN requests. When a valid request comes in,
 * a child TCB is made for the new connection and is placed in a
 * SYN queue. When the 3-way handshake is complete, it is moved to
 * an accept queue.
 * * @param *arg A `listen_args_t` struct.
 */
void* begin_listen(void *arg);

/**
 * @brief An application-side function that is called when the app is ready
 * to receive connection requests.
 * 
 * The SYN and accept queues are initialized and the listen socket's state is
 * set to `LISTEN`.
 * @param utcp_fd The listen socket's TCB FD
 * @param backlog The maximum number of TCBs that each queue can store at a time
 */
int utcp_listen(int utcp_fd, int backlog);

/**
 * @brief An application-side function that is called when the app wants to accept
 * a connection request.
 * @param listen_tcb The listening socket's TCB.
 * @param *client_addr The client's info will be stored in here.
 * 
 * @returns `-1` for invalid socket, or `int fd`, a UTCP fd, on success.
 */
int utcp_accept(tcb_t *listen_tcb, struct sockaddr_in *client_addr);

/*End function declarations*/

#endif
