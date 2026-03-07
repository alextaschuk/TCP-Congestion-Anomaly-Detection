#include <utcp/api/conn.h>

#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>

#include <tcp/hndshk_fsm.h>
#include <utils/err.h>
#include <utils/printable.h>
#include <utils/logger.h>
#include <utcp/api/api.h>
#include <utcp/api/globals.h>
#include <utcp/api/utcp_timers.h>


int udp_sock_open = 0; // changes to 1 when UDP socket is bound.

int bind_UDP_sock(int pts)
{
    if (udp_sock_open == 1)
        err_sock(-1, "[bind_UDP_sock] socket already bound");

    // declare socket -- sock = socket file descriptor (int that refers to the socket obj in the kernel)
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (sock == -1)
        err_sock(sock, "[bind_UDP_sock] failed to initialize socket");
    
    // prevent "address already in use" message when trying to rerun the program
    int yes = 1;
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
        err_sock(sock, "[bind_UDP_sock] setsockopt");

    struct sockaddr_in addr = 
    {
        .sin_family = AF_INET,
        .sin_port = htons(pts), // 0 means the kernel chooses a port for us
        .sin_addr.s_addr = inet_addr("127.0.0.1"), // localhost
      //.sin_addr.s_addr = htonl(INADDR_ANY), // accept datagrams at any of the machine's IP addresses
    };

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        err_sock(sock, "[bind_UDP_sock] bind failed");

    // get the bound port
    struct sockaddr_in bound_addr;
    socklen_t len = sizeof(bound_addr);
    getsockname(sock, (struct sockaddr*)&bound_addr, &len);

    LOG_INFO("[bind_UDP_sock] UDP socket bound to port %d", ntohs(bound_addr.sin_port));
    udp_sock_open = 1;
    return sock;
}

int bind_UTCP_sock(struct sockaddr_in *addr)
{
    api_t *global = api_instance();
    tcb_t *tcb = alloc_new_tcb();

    // TODO: dynamically select client/server UTCP port number
     // also need to validate src port and ip
    tcb->fourtuple.source_port = ntohs(addr->sin_port); // src UTCP port
    tcb->fourtuple.source_ip = ntohl(addr->sin_addr.s_addr);
    //tcb->fourtuple.source_ip = ntohl(addr->sin_addr.s_addr); // src IP addr
    tcb->fsm_state = CLOSED;
    
    global->tcb_lookup[tcb->fd] = tcb;

    LOG_INFO("[bind_UTCP_sock] UTCP socket bound to port: %i", tcb->fourtuple.source_port);
    return tcb->fd;
}

tcb_t *alloc_new_tcb(void)
{
    int fd;
    api_t *global = api_instance();

    /* Find the first available spot in the lookup table */
    for (fd = 0; fd < MAX_CONNECTIONS; fd++)
        if (global->tcb_lookup[fd] == NULL)
            break;

    if (fd == MAX_CONNECTIONS) {
        LOG_WARN("[allocate_new_tcb] Error: No sockets available in global table");
        return NULL; 
    }

    /* Create a new TCB and put it in the available spot */
    tcb_t *new_tcb = calloc(1, sizeof(tcb_t));
    if (!new_tcb)
        err_sys("[allocate_new_tcb] Failed to calloc *new_tcb");

    pthread_mutex_init(&new_tcb->lock, NULL);
    pthread_cond_init(&new_tcb->conn_cond, NULL);

    LOG_INFO("[allopc_new_tcb] Locking the new TCB...");
    pthread_mutex_lock(&new_tcb->lock);
    new_tcb->fd = fd; 
    new_tcb->fsm_state = CLOSED;

    // set congestion avoidance & control variables
    new_tcb->srtt = 0; // no RTT measurements have been made yet for this connection
    new_tcb->rttvar = 0;
    new_tcb->rto = 1000; // 1000 ms = 1 second
    new_tcb->rxtcur = 0; // TODO: calculate and replace w/ current RTO 
    new_tcb->dupacks = 0;
    
    new_tcb->snd_cwnd = MSS * 10;
    new_tcb->snd_ssthresh = BUF_SIZE;
    
    LOG_INFO("[allopc_new_tcb] Unlocking the new TCB...");
    pthread_mutex_unlock(&new_tcb->lock);
    
    global->tcb_lookup[fd] = new_tcb;
    return new_tcb;
}


tcb_t *find_listen_tcb(void)
{
    api_t *global = api_instance();

    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        tcb_t *curr_tcb = global->tcb_lookup[i];

        if (curr_tcb == NULL)
            continue;

        if (curr_tcb->fsm_state == LISTEN)
            return curr_tcb;
    }
    return NULL;
}
