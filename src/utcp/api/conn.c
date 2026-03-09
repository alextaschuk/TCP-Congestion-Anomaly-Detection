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

int bind_udp_sock(int pts)
{
    if (udp_sock_open == 1)
        err_sock(-1, "[bind_udp_sock] socket already bound");

    struct sockaddr_in addr = 
    {
        .sin_family = AF_INET,
        .sin_port = htons(pts), // 0 means the kernel chooses a port for us
        //.sin_addr.s_addr = inet_addr("127.0.0.1"), // localhost
        .sin_addr.s_addr = htonl(INADDR_ANY), // accept datagrams at any of the machine's IP addresses
    };

    // declare socket -- sock = socket file descriptor (int that refers to the socket obj in the kernel)
    int udp_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_fd == -1)
        err_sock(udp_fd, "[bind_udp_sock] failed to initialize socket");
    
    // prevent "address already in use" message when trying to rerun the program
    int yes = 1;
    if(setsockopt(udp_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
        err_sock(udp_fd, "[bind_udp_sock] setsockopt");

    if (bind(udp_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        err_sock(udp_fd, "[bind_udp_sock] bind failed");

    // get the bound port
    struct sockaddr_in bound_addr;
    socklen_t addrlen = sizeof(bound_addr);

    if (getsockname(udp_fd, (struct sockaddr *)&bound_addr, &addrlen) < 0)
        err_sys("getsockname failed");

    LOG_INFO("[bind_udp_sock] UDP socket bound to port %d. fd=%d (host order)", ntohs(bound_addr.sin_port), udp_fd);
    udp_sock_open = 1;
    return udp_fd;
}

int bind_utcp_sock(struct sockaddr_in *addr)
{
    api_t *global = api_instance();
    tcb_t *tcb = alloc_new_tcb();

    // TODO: dynamically select client/server UTCP port number
     // also need to validate src port and ip
    
    //pthread_mutex_lock(&tcb->lock);
    tcb->fourtuple.source_port = ntohs(addr->sin_port); // src UTCP port
    tcb->fourtuple.source_ip = ntohl(addr->sin_addr.s_addr);
    //tcb->fourtuple.source_ip = ntohl(addr->sin_addr.s_addr); // src IP addr
    tcb->fsm_state = CLOSED;

    LOG_INFO("[bind_utcp_sock] UTCP socket bound to port: %i", tcb->fourtuple.source_port);
    //pthread_mutex_unlock(&tcb->lock);
    
    return tcb->fd;
}

tcb_t *alloc_new_tcb(void)
{
    int fd;
    api_t *global = api_instance();

    pthread_mutex_lock(&global->lookup_lock);
    LOG_INFO("[alloc_new_tcb] Locked the TCB lookup table to search for an avalaible spot");
    /* Find the first available spot in the lookup table */
    for (fd = 0; fd < MAX_CONNECTIONS; fd++)
        if (global->tcb_lookup[fd] == NULL)
            break;

    if (fd == MAX_CONNECTIONS) {
        LOG_WARN("[alloc_new_tcb] No sockets available in lookup table. Unlocking the lookup table...");
        pthread_mutex_unlock(&global->lookup_lock);
        return NULL; 
    }

    /* Create a new TCB and put it in the available spot */
    tcb_t *new_tcb = calloc(1, sizeof(tcb_t));
    if (!new_tcb)
        err_sys("[alloc_new_tcb] Failed to calloc *new_tcb");

    global->tcb_lookup[fd] = new_tcb;

    LOG_INFO("[alloc_new_tcb] Added the new TCB to the lookup table. Unlocking the lookup table...");
    pthread_mutex_unlock(&global->lookup_lock);

    pthread_mutex_init(&new_tcb->lock, NULL);
    pthread_cond_init(&new_tcb->conn_cond, NULL);

    //LOG_INFO("[alloc_new_tcb] Locking the new TCB to initialize its variables...", new_tcb->fd);
    //pthread_mutex_lock(&new_tcb->lock);
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
    
    LOG_INFO("[alloc_new_tcb] Finished initializing the TBC with fd=%i.", new_tcb->fd);
    log_tcb(new_tcb, "[alloc_new_tcb] New TCB:");
    
    //LOG_INFO("[alloc_new_tcb] Unlocking the new TCB...");
    //pthread_mutex_unlock(&new_tcb->lock);
    
    return new_tcb;
}


tcb_t *find_listen_tcb(void)
{
    api_t *global = api_instance();

    pthread_mutex_lock(&global->lookup_lock);
    LOG_DEBUG("[find_listen_tcb] Locked the TCB lookup table to search for the listen socket's TCB...");

    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        tcb_t *curr_tcb = global->tcb_lookup[i];

        if (curr_tcb == NULL)
            continue;

        if (curr_tcb->fsm_state == LISTEN)
        {
            LOG_DEBUG("[find_listen_tcb] Found the listen socket's TCB in the lookup table at fd=%i. Unlocking the TCB lookup table...", i);
            pthread_mutex_unlock(&global->lookup_lock);
            return curr_tcb;
        }
    }
    LOG_WARN("[find_listen_tcb] Listen socket's TCB not found in the lookup table. Unlocking the TCB lookup table...");
    return NULL;
}
