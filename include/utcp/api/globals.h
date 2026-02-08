/*
Contains global API macros, variables,
structs, etc. that both the server and
client will need access to.
*/
#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdint.h>
#include <tcp/tcb.h>

/* Define Macros*/
#define MAX_UTCP_SOCKETS 1024
/* End define macros*/

/* Define Enums*/
/* End define Enums*/

// struct to hold all global vars
typedef struct 
{
// client info
uint16_t client_port;
int client_utcp_port; 
char* client_ip;
// server info
uint16_t server_port;
int server_utcp_port;
char* server_ip;

struct tcb *tcp_lookup[MAX_UTCP_SOCKETS];
} api_t;

api_t *api_instance(void); // to access the global struct

#endif