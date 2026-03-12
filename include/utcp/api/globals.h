/*
Contains global API macros, variables, structs, etc. that both the server
and client will need access to.
*/
#ifndef GLOBALS_H
#define GLOBALS_H

#include <netinet/in.h>
#include <stdint.h>

#include <pthread.h>

struct tcb_t;
typedef struct tcb_t tcb_t;


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
#define BUF_SIZE 1048576

/**
 * The default Maximum Segment Size value of a TCP segment.
 * @note See https://www.rfc-editor.org/rfc/rfc9293#name-maximum-segment-size-option
 */
#define MSS 536

/* Congestion Control Related */
#define CA_ALGO RENO /* Determine which CA algo we use. */

/* Timer Stuff*/
#define TCPT_NTIMERS 5  /* number of counters in `t_timer[]` */

/* Retransmission Timer Stuff */

#define TCPTV_MIN 2                     /* minimum value of retransmission timer (1 sec)*/
#define TCPTC_REXMTMAX 128              /* maximum value of retransmission timer (64 sec)*/
#define MAXRXTSHIFT 12                  /* maximum number of retransmissions waiting for an ACK */

 /* Persist Timer Stuff*/
#define TCPTV_PERSMIN 10                /* minimum value of persist timer (5 sec)*/
#define TCPTV_PERSMAX 120               /* maximum value of retransmission timer (60 sec)*/

 /* Convinience macros */
#define MIN(a,b) ((a) < (b) ? (a) : (b)) /* Return the smaller value between `a` and `b` */
#define MAX(a,b) ((a) > (b) ? (a) : (b)) /* Return the larger value between `a` and `b` */
#define SEQ_LT(a, b)  ((int)((a) - (b)) < 0)
#define SEQ_LEQ(a, b) ((int)((a) - (b)) <= 0)
#define SEQ_GT(a, b)  ((int)((a) - (b)) > 0)
#define SEQ_GEQ(a, b) ((int)((a) - (b)) >= 0)

/* End define macros */

/*Define Structs*/
typedef struct api_t /* Stores all global vars */
{
    /* Connection info */
    uint16_t client_udp_port;
    uint16_t server_udp_port;

    uint16_t udp_port;
    int udp_fd;
    int utcp_fd;

    struct sockaddr_in client;
    struct sockaddr_in server;

    tcb_t *tcb_lookup[MAX_CONNECTIONS]; /* TCB lookup table. Contains all connections. */
    pthread_mutex_t lookup_lock;        /* Mutex to lock iterating over lookup table. */

} api_t;


typedef struct socket_fds /* A helpful struct to consolidate our socket FDs for multithreading */
{
    int udp_fd;                         /* A UDP file descriptor */
    int utcp_fd;                        /* A UTCP file descriptor */
} socket_fds;
/*End define structs*/

/*Define Enums*/
enum cc_algos{ /* Congestion control algorithms */
    TAHOE = 1,
    RENO = 2
};

enum timers{
    TCPT_REXMT =   0,  /* index of retransmission timer in `timer_t[]` */
    TCPT_PERSIST = 1,  /* Persist timer (for zero window probes) */
    TCPT_KEEP =    2,  /* Keepalive timer */
    TCPT_2MSL =    3,  /* 2*MSL timer (TIME_WAIT state) */
    TCPT_DELACK =  4  /* Delayed ACK timer */
};
/*End define enums*/

/*Define Variables*/

/**
 * A thread-local variable that tells us which thread is logging something.
 */
extern _Thread_local const char* current_thread_cat;
/*End define variables*/

api_t *api_instance(void);              /* Call to access the global struct's contents */

#endif