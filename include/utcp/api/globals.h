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

/**
 * @brief The maximum number of UTCP socket connections allowed at a time.
 * 
 * This defines how many TCB structs our lookup table can store
 */
//#define MAX_UTCP_SOCKETS 1024
#define MAX_UTCP_SOCKETS 3

/**
 * The maximum number of allowed TCBs in SYS & accept queues at a time.
 * 
 * @note This is typically determined with SOMAXCONN (SOcket MAXimum CONNections), but my system's stores
 * 128 in this value, and I want to use more. See https://www.kernel.org/doc/Documentation/networking/ip-sysctl.txt
 * 
 */
#define MAX_BACKLOG 4096 

 // buffer & data related

 /**
  * The number of bytes our `rx` and `tx` buffers can hold
  */
#define BUF_SIZE 1024

/**
 * The default Maximum Segment Size value of a TCP segment.
 * @note see https://www.rfc-editor.org/rfc/rfc9293#name-maximum-segment-size-option
 */
#define MSS 536

 // convinience macros
#define MIN(a,b) ((a) < (b) ? (a) : (b)) // return the smaller value between `a` and `b`

/* End define macros*/

/* Define Enums*/
/* End define Enums*/

/*Define Structs*/

/**
 * @brief A helpful struct to consolidate our socket
 * FDs for multithreading.
 * 
 * @param udp_fd A UDP socket FD
 * @param utcp_fd A UTCP socket FD
 * @note The FDs stored in this struct are for the listen socket.
 * 
 */
typedef struct listen_args_t
{
    int udp_fd;
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
typedef struct api_t
{
// client info
uint16_t client_port;
int client_utcp_port; 
char* client_ip;
// server info
uint16_t server_port;
int server_utcp_port;
char* server_ip;

tcb_t *tcb_lookup[MAX_UTCP_SOCKETS]; // tcb lookup table

tcb_queue_t syn_queue;
tcb_queue_t accept_queue;
//app_bufs_t *app_bufs;
} api_t;

api_t *api_instance(void); // to access the global struct

#endif