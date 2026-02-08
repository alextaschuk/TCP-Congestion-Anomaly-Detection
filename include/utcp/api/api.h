#ifndef API_H
#define API_H

#include <stdint.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>


#include <tcp/hndshk_fsm.h>
#include <tcp/tcb.h>



/*Begin function declarations*/
void deserialize_tcp_hdr(uint8_t* buf, size_t buflen, struct tcphdr **out_hdr, uint8_t **out_data, ssize_t *out_data_len);
void update_fsm(int utcp_fd, enum conn_state state);

struct tcb* get_tcb(int utcp_fd);
/*End function declarations*/
#endif
