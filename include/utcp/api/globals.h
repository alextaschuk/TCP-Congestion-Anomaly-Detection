/*
Contains global API macros, variables,
structs, etc. that both the server and
client will need access to.
*/
#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdint.h>
#include <tcp/tcb.h>

/* Define Macros */

 /*socket-related*/
#define MAX_CONNECTIONS 1024 /* The maximum number of UTCP socket connections allowed at a time (in the lookup table). */
/**
 * The maximum number of allowed TCBs in SYS & accept queues at a time.
 * @note This is typically determined with `SOMAXCONN` (SOcket MAXimum CONNections), but my system's stores
 * 128 in this value, and I want to use more. See https://www.kernel.org/doc/Documentation/networking/ip-sysctl.txt
 */
#define MAX_BACKLOG 4096 

 /*Buffer & Data Related*/

/**
 * The number of bytes our `rx` and `tx` buffers can hold
 * @note See https://en.wikipedia.org/wiki/TCP_tuning#Buffers
 */
#define BUF_SIZE 65535

/**
 * The default Maximum Segment Size value of a TCP segment.
 * @note See https://www.rfc-editor.org/rfc/rfc9293#name-maximum-segment-size-option
 */
#define MSS 536

/* Congestion Control  Related */
//#define TCPT_NTIMERS 4                  /* number of counters in `t_timer[]` */

 /* Retransmission Timer Stuff */
#define TCPT_REXMT 0                    /* index of retransmission timer in `timer_t[]` */
#define TCPTV_MIN 2                     /* minimum value of retransmission timer (1 sec)*/
#define TCPTC_REXMTMAX 128              /* maximum value of retransmission timer (64 sec)*/
#define MAXRXTSHIFT 12                  /* maximum number of retransmissions waiting for an ACK */

 /* Persist Timer Stuff*/
#define TCPTV_PERSMIN 10                /* minimum value of persist timer (5 sec)*/
#define TCPTV_PERSMAX 120               /* maximum value of retransmission timer (60 sec)*/

 /* Convinience macros */
#define MIN(a,b) ((a) < (b) ? (a) : (b)) /* Return the smaller value between `a` and `b` */
#define MAX(a,b) ((a) > (b) ? (a) : (b)) /* Return the larger value between `a` and `b` */
/* End define macros */

/*Define Structs*/
typedef struct socket_fds               /* A helpful struct to consolidate our socket FDs for multithreading */
{
    int udp_fd;                         /* A UDP file descriptor */
    int utcp_fd;                        /* A UTCP file descriptor */
} socket_fds;
/*End define structs*/


typedef struct api_t /* Stores all global vars */
{
    /* client info */

    uint16_t client_port;
    int client_utcp_port; 
    char* client_ip;

    /* server info */

    uint16_t server_port;
    int server_utcp_port;
    char* server_ip;

    tcb_t *tcb_lookup[MAX_CONNECTIONS]; /* TCB lookup table. Contains all connections. */
} api_t;

api_t *api_instance(void);              /* Call to access the global struct's contents */

#endif