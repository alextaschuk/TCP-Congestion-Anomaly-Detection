#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "../pdp/pdp.h"

/* Define functions */
void build_udp_pkt(char *header, char *data);
Pdp_header make_hedr(uint32_t seq, uint32_t ack);
void send_syn(char* buf);
/* End define functions */