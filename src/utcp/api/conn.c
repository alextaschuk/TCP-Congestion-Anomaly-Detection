#include <utcp/api/conn.h>

#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>

#include <tcp/congestion_control.h>
#include <tcp/hndshk_fsm.h>
#include <utils/err.h>
#include <utils/printable.h>
#include <utils/logger.h>
#include <utcp/api/api.h>
#include <utcp/api/globals.h>
#include <utcp/api/utcp_timers.h>
#include <utcp/rx/rx_dgram.h>


int udp_sock_open = 0; // changes to 1 when UDP socket is bound to a port.


uint16_t bind_udp_sock(int pts)
{
    api_t *global = api_instance();

    if (udp_sock_open)
    {
        LOG_DEBUG("[init_host] UTCP socket already bound. Skipping init.");
        return global->udp_fd;
    }

    if (udp_sock_open == 1)
        err_sock(-1, "[bind_udp_sock] socket already bound");

    struct sockaddr_in addr = 
    {
        .sin_family = AF_INET,
        .sin_port = htons(pts), // 0 means the kernel chooses a port for us
        .sin_addr.s_addr = htonl(INADDR_ANY), // accept datagrams at any of the machine's IP addresses
    };

    // declare socket -- sock = socket file descriptor (int that refers to the socket obj in the kernel)
    int udp_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_fd == -1)
        err_sock(udp_fd, "[bind_udp_sock] failed to initialize socket");
    
    /* prevent "address already in use" message when trying to rerun the program */
    int yes = 1;
    if(setsockopt(udp_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
        err_sock(udp_fd, "[bind_udp_sock] setsockopt");

    if (bind(udp_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        err_sock(udp_fd, "[bind_udp_sock] bind failed");

    /* get the bound port */
    struct sockaddr_in bound_addr;
    socklen_t addrlen = sizeof(bound_addr);

    if (getsockname(udp_fd, (struct sockaddr *)&bound_addr, &addrlen) < 0)
        err_sys("getsockname failed");

    LOG_INFO("[bind_udp_sock] UDP socket bound to port %d, fd=%d", ntohs(bound_addr.sin_port), udp_fd);

    global->udp_fd = udp_fd;
    global->udp_port = ntohs(bound_addr.sin_port);

    spawn_threads(global);

    udp_sock_open = 1;

    return ntohs(bound_addr.sin_port);
}


int init_utcp_sock(void)
{
    bind_udp_sock(1515);

    tcb_t *tcb = alloc_new_tcb();
    return tcb->fd;
}


int bind_utcp_sock(int utcp_fd, struct sockaddr_in *addr)
{
    api_t *global = api_instance();

    tcb_t *tcb = get_tcb(utcp_fd);
    if (!tcb)
        err_sys("[bind_utcp_sock] TCB not found");

    //pthread_mutex_lock(&tcb->lock);
    tcb->fourtuple.source_port = ntohs(addr->sin_port); // src UTCP port
    tcb->fourtuple.source_ip = ntohl(addr->sin_addr.s_addr);

    tcb->src_udp_fd = global->udp_fd;
    tcb->src_udp_port = global->udp_port;

    //pthread_mutex_unlock(&tcb->lock);
    LOG_INFO("[bind_utcp_sock] Bound UTCP fd=%d to UTCP port=%u", utcp_fd, tcb->fourtuple.source_port);
    return tcb->fd;
}


tcb_t *alloc_new_tcb(void)
{
    int fd;
    api_t *global = api_instance();

    pthread_mutex_lock(&global->lookup_lock);
    //LOG_INFO("[alloc_new_tcb] Locked the TCB lookup table to search for an avalaible spot");
    /* Find the first available spot in the lookup table */
    for (fd = 0; fd < MAX_CONNECTIONS; fd++)
        if (global->tcb_lookup[fd] == NULL)
            break;

    if (fd == MAX_CONNECTIONS) {
        //LOG_WARN("[alloc_new_tcb] No sockets available in lookup table. Unlocking the lookup table...");
        pthread_mutex_unlock(&global->lookup_lock);
        return NULL; 
    }

    /* Create a new TCB and put it in the available spot */
    tcb_t *new_tcb = calloc(1, sizeof(tcb_t));
    if (!new_tcb)
        err_sys("[alloc_new_tcb] Failed to calloc *new_tcb");

    global->tcb_lookup[fd] = new_tcb;

    //LOG_INFO("[alloc_new_tcb] Added the new TCB to the lookup table. Unlocking the lookup table...");
    pthread_mutex_unlock(&global->lookup_lock);

    pthread_mutex_init(&new_tcb->lock, NULL);
    pthread_cond_init(&new_tcb->conn_cond, NULL);

    //LOG_INFO("[alloc_new_tcb] Locking the new TCB to initialize its variables...", new_tcb->fd);
    //pthread_mutex_lock(&new_tcb->lock);
    new_tcb->fd = fd; 
    new_tcb->fsm_state = CLOSED;

    new_tcb->iss = 0;
    new_tcb->snd_una = new_tcb->iss;
    new_tcb->snd_nxt = new_tcb->iss;
    new_tcb->snd_max = new_tcb->iss;
    new_tcb->rcv_wnd = BUF_SIZE;

    uint8_t scale = 0;
    while (BUF_SIZE >> scale > 65535 && scale < 14)
    {
        scale++;
    }

    new_tcb->rcv_ws_scale = scale;
    new_tcb->ws_enabled = false;
    new_tcb->snd_ws_scale = 0;

    // set congestion avoidance & control variables
    new_tcb->rxtcur = TCPTV_SRTTDFLT;
    new_tcb->srtt = 0; // no RTT measurements have been made yet for this connection
    new_tcb->rttvar = 0;

    switch(CC_ALGO)
    {
        case (TAHOE):
            //new_tcb->cc = &cc_tahoe_ops;
            break;
        case (RENO):
            //new_tcb->cc = &cc_reno_ops;
            break;
        case(NEW_RENO):
            new_tcb->cc = &cc_newreno_ops;
            break;
        default:
            err_sys("[alloc_new_tcb] ERROR: Invalid CC algorithm");
            break;
    }
    new_tcb->cc->init(new_tcb);
    
    
    LOG_INFO("[alloc_new_tcb] Finished initializing the TBC with fd=%i.", new_tcb->fd);
    //log_tcb(new_tcb, "[alloc_new_tcb] New TCB:");

    
    //LOG_INFO("[alloc_new_tcb] Unlocking the new TCB...");
    //pthread_mutex_unlock(&new_tcb->lock);
    
    return new_tcb;
}


tcb_t *find_listen_tcb(void)
{
    api_t *global = api_instance();

    pthread_mutex_lock(&global->lookup_lock);
    //LOG_DEBUG("[find_listen_tcb] Locked the TCB lookup table to search for the listen socket's TCB...");

    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        tcb_t *curr_tcb = global->tcb_lookup[i];

        if (curr_tcb == NULL)
            continue;

        if (curr_tcb->fsm_state == LISTEN)
        {
            //LOG_DEBUG("[find_listen_tcb] Found the listen socket's TCB in the lookup table at fd=%i. Unlocking the TCB lookup table...", i);
            pthread_mutex_unlock(&global->lookup_lock);
            return curr_tcb;
        }
    }
    LOG_WARN("[find_listen_tcb] Listen socket's TCB not found in the lookup table. Unlocking the TCB lookup table...");
    return NULL;
}


int spawn_threads(api_t *global)
{
    pthread_t listen_thread;
    pthread_t ticker_thread;

    LOG_INFO("[spawn_threads] Spawning listener thread...");
    if (pthread_create(&listen_thread, NULL, utcp_listen_thread, global) != 0)
    {
        LOG_ERROR("[spawn_threads] Failed to create listener thread");
        return -1;
    }

    LOG_INFO("[spawn_threads] Spawning ticker thread...");
    if (pthread_create(&ticker_thread, NULL, utcp_ticker_thread, NULL) != 0)
    {
        LOG_INFO("[spawn_threads] Failed to create ticker thread");
        return -1;
    }
    return 0;
}


void *utcp_listen_thread(void *arg)
{
    current_thread_cat = "listen_thread";
    LOG_INFO("[utcp_listen_thread] Listen thread running...");
    
    api_t *global = (api_t *)arg;
    while (1)
    {
        ssize_t rcvsize = rcv_dgram(global->udp_fd, BUF_SIZE);
        if (rcvsize < 0)
            err_sys("[Server, listen thread] Error receiving packet");
    }
    return 0;
}