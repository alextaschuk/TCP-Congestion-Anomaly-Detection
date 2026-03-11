#ifndef FOURTUPLE_H
#define FOURTUPLE_H

#include <stdint.h>

/**
 * A standard fourtuple that is used to identify a 
 * TCP socket.
 * 
 * @note The source & destination ports store a 
 * connection's UTCP port.
 */
typedef struct fourtuple_t
{
    //standard 4-tuple
    uint16_t source_port; /* UTCP port */
    uint32_t source_ip;   /* Source IPv4 Address */
    uint16_t dest_port;   /* UTCP port */
    uint32_t dest_ip;     /* Destination IPv4 Address */
} fourtuple_t;

#endif // FOURTUPLE_H
