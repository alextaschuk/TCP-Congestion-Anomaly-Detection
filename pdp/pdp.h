#ifndef PDP_H
#define PDP_H

#include "fourtuple.h"


typedef struct Pdp_header
{
    struct fourtuple fourtuple;
    int seq;
    int ack; // Acknowledgement Number, not ACK flag
    int listen_port;
    int fsm_state;
}Pdp_header;
#endif // PDP_H