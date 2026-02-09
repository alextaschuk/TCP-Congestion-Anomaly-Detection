#ifndef CLIENT_H
#define CLIENT_H

#include <netinet/in.h>

/*Define variables*/
extern int client_fsm;
extern int sock;
extern int UTCP_sock;
extern struct sockaddr_in client_addr;
extern struct sockaddr_in server_addr;
/*End define variables*/

/* Define functions */
static void perform_hndshk(int sock, int utcp_fd);
static void set_server_port(void);
/* End define functions */
#endif
