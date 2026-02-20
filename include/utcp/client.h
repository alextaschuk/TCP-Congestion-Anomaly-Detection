#ifndef CLIENT_H
#define CLIENT_H

#include <netinet/in.h>

#include <tcp/tcb.h>
#include<utcp/api/globals.h>

/*Define variables*/

extern int client_fsm;
extern int sock;
extern int UTCP_sock;
extern struct sockaddr_in client_addr;
extern struct sockaddr_in server_addr;

/*End define variables*/

/* Define functions */

/**
 * @brief Initiate a 3-way handshake with the server.
 * 
 * This function sends a SYN packet, receives and
 * handles a SYN-ACK packet, then responds with an
 * ACK packet.
 */
static void init_hndshk(void *arg);

void* begin_listen(void *arg);

/**
 * Initializes the client by binding its UDP and UTCP sockets,
 * then updates the client's TCB with the (dest) server's info
 */
static int init_client(void *arg, api_t *global);


/**
 * @brief allows us to enter the server's port
 * number if we don't hardcode the value.
 */
static void set_server_port(void);

/* End define functions */
#endif
