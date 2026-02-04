#ifndef FOURTUPLE_H
#define FOURTUPLE_H

typedef struct fourtuple
{
    //standard 4-tuple
    uint16_t source_port; // UTCP port
    uint32_t source_ip;
    uint16_t dest_port; // UTCP port
    uint32_t dest_ip;
}fourtuple;
#endif // FOURTUPLE_H