#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


/* Define functions */
void build_udp_pkt(uint8_t* header, uint8_t *data);
static void set_server_port();

static void perform_hndshk(int sock, int utcp_fd);
/* End define functions */