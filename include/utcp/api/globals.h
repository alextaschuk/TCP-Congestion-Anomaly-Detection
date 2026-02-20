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

// socket-related
#define MAX_UTCP_SOCKETS 1024 // max number of utcp socket connections allowed at a time

// buffer & data related
#define BUF_SIZE 1024 // size of send and receive buffers
#define MSS 536 // default MSS size (see https://www.rfc-editor.org/rfc/rfc9293#section-3.7.1-3)

// convinience macros 
#define MIN(a,b) ((a) < (b) ? (a) : (b)) // return the smaller value between `a` and `b`
/* End define macros*/

/* Define Enums*/
/* End define Enums*/

/*Define Structs*/

/**
 * A helpful struct to consolidate
 * our socket FDs for multithreading.
 */
typedef struct listen_args_t
{
    int udp_sock;
    int utcp_fd;
} listen_args_t;

/**
 * This struct contains a send and receive
 * buffer so that we can mock an application
 * sending and receiving data from our
 * client/server's `rx` and `tx` buffers.
 */
//typedef struct app_bufs_t
//{
//    uint8_t rcv_buf;
//    uint8_t snd_buf;
//
//} app_bufs_t;
/*End define structs*/

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

tcb_t *tcp_lookup[MAX_UTCP_SOCKETS]; // tcb lookup table
//app_bufs_t *app_bufs;

} api_t;

api_t *api_instance(void); // to access the global struct

#endif