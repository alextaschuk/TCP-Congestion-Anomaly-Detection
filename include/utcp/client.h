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

void* begin_rcv(void *arg);

/**
 * Initializes the client by binding its UDP and UTCP sockets,
 * then updates the client's TCB with the (dest) server's info
 */
static void init_client(void *arg, api_t *global);


/** 
 * @brief A application-side function that is used to establish a UTCP connection.
 * 
 * This function creates a new TCB for the client and initializes its necessary values.
 * 
 * @return The FD of the newly-created TCB.
 */
static int utcp_connect(int udp_fd, const struct sockaddr_in *dest_addr);

/**
 * Spawns the receive and ticker threads.
 */
static int spawn_threads(socket_fds *args);

/* End define functions */
#endif
