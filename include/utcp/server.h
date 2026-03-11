#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <utcp/api/globals.h>


/* Define macros */
#define SYN_BACKLOG 128 // max number of connections in SYN queue during 3WHS
#define ACCEPT_BACKLOG 20 // max number of connections in incoming queue for `accept()`
/*End define macros*/

/* Define variables */
extern uint8_t header[8]; //datagram header
extern uint8_t data[]; //buffer to send data
/* End define variables */

/* Begin function declarations */

/**
 * @brief An application-side function to tell us (pretending we're the OS)
 * that the app is ready to receive connection requests.
 * 
 * The SYN and accept queues are initialized and the listen socket's state is
 * set to `LISTEN`.
 * @param utcp_fd The listen socket's TCB FD
 * @param backlog The maximum number of TCBs that each queue can store at a time
 */
int utcp_listen(api_t *global, int backlog);

/**
 * @brief An application-side function that is called when the app wants to accept
 * a connection request that is sitting in the accept queue.
 * 
 * @param *client_addr The client's info will be stored in here.
 * 
 * @returns `-1` for invalid socket, or `int fd`, a UTCP fd, on success.
 */
int utcp_accept(api_t *global, struct sockaddr_in *client_addr);


/* End function declarations */

#endif
