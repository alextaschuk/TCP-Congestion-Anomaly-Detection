#ifndef FOURTUPLE_H
#define FOURTUPLE_H

#include <stdint.h>


typedef struct fourtuple_t
{
    //standard 4-tuple
    uint16_t source_port; // UTCP port
    uint32_t source_ip;
    uint16_t dest_port; // UTCP port
    uint32_t dest_ip;
} fourtuple_t;

#endif // FOURTUPLE_H
