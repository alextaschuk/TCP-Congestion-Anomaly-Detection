/*
Logic related to connection establishment and management,
whether that be for a UDP socket, a UTCP socket, or anything
else.
*/
#include <utcp/api/conn.h>

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include <utcp/api/globals.h>
#include <utcp/api/api.h>

#include <tcp/tcb.h>
#include <tcp/hndshk_fsm.h>

#include <utils/err.h>

int udp_sock_open = 0; // 1 if UDP socket is bound

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
    
//  *pts = ntohs(bound_addr.sin_port);

    printf("UDP socket bound to port %d\n", ntohs(bound_addr.sin_port));
    udp_sock_open = 1;
    return sock;
}

int bind_UTCP_sock(struct sockaddr_in *addr)
{
    int fd;
    api_t *global = api_instance();

    // is it safe to have irs & rcv_nxt zeroed out?
    tcb_t *tcb = calloc(1, sizeof(tcb_t));

    if (!tcb)
        err_sys("[bind_UTCP_sock]failed to calloc *tcb");
    if (addr->sin_port == 0)
        err_sys("[bind_UTCP_sock]client UDP socket not bound before UTCP binding");

    // find the first available spot in the lookup table
    for(fd = 0; fd < MAX_UTCP_SOCKETS; fd++)
        if (global->tcp_lookup[fd] == NULL)
            break; // found an available socket
    
    if (fd == MAX_UTCP_SOCKETS || fd == -1)
    {
        free(tcb);
        err_sys("[bind_UTCP_sock]no socket available");
    }

    // TODO: dynamically select client/server UTCP port number
     // also need to validate src port and ip
    tcb->fourtuple.source_port = ntohs(addr->sin_port); // src UTCP port
    tcb->fourtuple.source_ip = ntohl(addr->sin_addr.s_addr);
    //tcb->fourtuple.source_ip = ntohl(addr->sin_addr.s_addr); // src IP addr
    tcb->fsm_state = CLOSED;
    
    global->tcp_lookup[fd] = tcb;

    printf("UTCP socket bound to port: %i\n", tcb->fourtuple.source_port);
    return fd;
}
