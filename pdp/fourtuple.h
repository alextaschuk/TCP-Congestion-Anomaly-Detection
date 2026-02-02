#ifndef FOURTUPLE_H
#define FOURTUPLE_H

typedef struct fourtuple
{
    //standard 4-tuple
    int source_port;
    char* source_ip;
    int dest_port;
    char* dest_ip;
}fourtuple;
#endif // FOURTUPLE_H