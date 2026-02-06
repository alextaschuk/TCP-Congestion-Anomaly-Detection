/**
 * @brief This API contains functions, variables, etc. that are used 
 * by the server and the client.
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/tcp.h>

//#include <api.h>
//#include <tcb.h>
//#include <hndshk_fsm.h>
//#include <fourtuple.h>
//#include <tcphdr.h>
//#include <tcp_segment.h>
#include "api.h"
#include "tcp_segment.h"
#include "../utils/debug.c"

uint16_t client_port = 5555;
const int client_utcp_port = 776; 
const char* client_ip = "127.0.0.1";

uint16_t server_port = 4567;
const int server_utcp_port = 332;
const char* server_ip = "127.0.0.1";

struct tcb* tcb_lookup[MAX_UTCP_SOCKETS];
int udp_sock_open = 0; // 1 if UDP socket is bound


int bind_UDP_sock(int pts)
{
    /**
     * @brief create and return a socket descriptor for 
     * an Internet datagram socket using UDP.
     * 
     * @param pts (port to set), the client passes in the 
     * client_port, and the server passes in the server_port variable
     * to dynamically set the values to the port that the kernel
     * chooses. If we hardcode the port, this value is not needed.
     */
    
    if (udp_sock_open)
        err_sock(-1, "(bind_UDP_sock)socket already bound");

    // declare socket -- sock = socket file descriptor (int that refers to the socket obj in the kernel)
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (sock == -1)
        err_sock(sock, "(bind_UDP_sock)failed to initialize socket");
    
    // prevent "address already in use" message when trying to rerun the program
    int yes = 1;
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
        err_sock(sock, "(bind_UDP_sock)setsockopt");

    struct sockaddr_in addr = 
    {
        .sin_family = AF_INET,
        .sin_port = htons(pts), // 0 means the kernel chooses a port for us
        .sin_addr.s_addr = inet_addr("127.0.0.1"), // localhost
      //.sin_addr.s_addr = htonl(INADDR_ANY), // accept datagrams at any of the machine's IP addresses
    };

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        err_sock(sock, "(bind_UDP_sock)bind failed");

    // get the bound port
    struct sockaddr_in bound_addr;
    socklen_t len = sizeof(bound_addr);
    getsockname(sock, (struct sockaddr*)&bound_addr, &len);
    
//    *pts = ntohs(bound_addr.sin_port);

    printf("UDP socket bound to port %d\n", ntohs(bound_addr.sin_port));
    udp_sock_open = 1;
    return sock;
}


int bind_utcp(struct sockaddr_in *addr)
{
    /**
     * @brief We need to manually manage a bind() function
     * since client sockets need to be bound according to
     * their UTCP port number, not the actual UDP port number.
     * 
     * @param addr contains socket's source IP address and port number
     * 
     * @return fd on success, a "file descriptor" (index of tcb_lookup) that the
     * newly created tcb struct lives at.
     */
    // find the first available spot in the lookup table
    int fd;
    for(fd = 0; fd < MAX_UTCP_SOCKETS; fd++)
        if (tcb_lookup[fd] == NULL)
            break; // found an available socket

        if (fd == MAX_UTCP_SOCKETS)
            err_sys("[bind_utcp]no socket available");

    struct tcb *tcb = calloc(1, sizeof(struct tcb));
    if (!tcb)
        err_sys("[bind_utcp]failed to calloc *tcb:");

    if (addr->sin_port == 0)
        err_sys("[bind_utcp]client UDP socket not bound before UTCP binding");
    // TODO: dynamically select client/server UTCP port number
        // also need to validate src port and ip
    tcb->fourtuple.source_port = ntohs(addr->sin_port); // src UTCP port
    tcb->fourtuple.source_ip = inet_addr("127.0.0.1");
    //tcb->fourtuple.source_ip = ntohl(addr->sin_addr.s_addr); // src IP addr
    tcb->fsm_state = CLOSED;

    tcb_lookup[fd] = tcb;
    printf("UTCP socket bound to port: %i\r\n", tcb->fourtuple.source_port);
    return fd;
}

void connect_utcp(int utcp_fd, struct sockaddr_in* addr, uint16_t dest_udp)
{
    /**
     * @brief connect a UTCP socket to a remote UTCP socket
     * 
     * @param utcp_fd the index of the local UTCP socket in tcb_lookup
     * @param addr contains the destination UTCP port number and IP address
     * @param dest_udp the destination UDP address
     */
    struct tcb *tcb = get_tcb(utcp_fd);
    

    //Update the TCB's info
    // TODO: validate port numbers and ip
    tcb->fourtuple.dest_port = ntohs(addr->sin_port); //dest UTCP port
    tcb->fourtuple.dest_ip = ntohl(addr->sin_addr.s_addr); // dest IP
    //tcb->fourtuple.dest_ip = ntohl(inet_addr("127.0.0.1")); // dest IP
    tcb->dest_udp_port = dest_udp; // dest UDP port number

    printf("Dest UTCP Port: %u, dest IP: %u, dest UDP port: %u\r\n", tcb->fourtuple.dest_port, tcb->fourtuple.dest_ip, tcb->dest_udp_port);

    tcb->iss = 0x0000; // initial seq # = 0
    tcb->snd_una = tcb->iss; // hasn't been ack'd b/c SYN not yet sent
    tcb->snd_nxt = tcb->iss;
}


void deserialize_tcp_hdr(uint8_t* buf, size_t buflen, tcphdr **out_hdr, uint8_t **out_data, ssize_t *out_data_len)
{
    /**
     * @brief deserialize a TCP header back into 
     * host byte order
     */
    if (buflen < sizeof(tcphdr))
        err_sys("[deserialize_tcp_hdr]cannot parse datagram");

    *out_hdr = (tcphdr *)buf;

    // Convert header fields from network byte order to host byte order
    (*out_hdr)->th_sport = ntohs((*out_hdr)->th_sport); // src port
    (*out_hdr)->th_dport = ntohs((*out_hdr)->th_dport); // dest port
    (*out_hdr)->th_seq = ntohl((*out_hdr)->th_seq); // seq #
    (*out_hdr)->th_ack = ntohl((*out_hdr)->th_ack); // ack #
    (*out_hdr)->th_win = ntohs((*out_hdr)->th_win); // rcv window size
    (*out_hdr)->th_sum = ntohs((*out_hdr)->th_sum); // checksum
    (*out_hdr)->th_urp = ntohs((*out_hdr)->th_urp); // urgent pointer

    //print_tcphdr(*out_hdr);
}

struct tcb* get_tcb(int utcp_fd)
{
    /**
     * @brief Retrieves the TCB for a UTCP socket at tcb_lookup[pos]
     */
    if (utcp_fd < 0 || utcp_fd >= MAX_UTCP_SOCKETS)
        err_sys("[get_tcb]Invalid lookup table position");
    return tcb_lookup[utcp_fd];
}

int send_dgram(int sock, int utcp_fd, void* buf, size_t len, int flags)
{
    /**
     * @brief send a buffer of data to a socket.
     * 
     * @param utcp_fd UTCP socket's position in tcb_lookup
     * @return bytes_sent the number of bytes sent in the
     * datagram, or -1 if fails
     */
    struct tcb *tcb = get_tcb(utcp_fd);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(tcb->dest_udp_port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // allocate for a segment (TCP header + data)
    size_t segment_size = sizeof(tcphdr) + len;
    tcp_segment *segment = malloc(segment_size);
    if (!segment)
        err_sys("[send_dgram]error allocating segment");
    
    memset(&segment->hdr, 0, sizeof(tcphdr));
    segment->hdr.th_sport = htons(tcb->fourtuple.source_port);
    segment->hdr.th_dport = htons(tcb->fourtuple.dest_port);
    segment->hdr.th_seq = htonl(tcb->snd_nxt);
    segment->hdr.th_ack = htonl(tcb->rcv_nxt);
    segment->hdr.th_off_flags = (sizeof(tcphdr) / 4) << 4; // Convert into 32-bit words
    segment->hdr.th_flags = flags;
    segment->hdr.th_win = htons(1024); // dummy window

    // add buffer to the payload
    memcpy(segment->data, buf, len);

    // send the datagram
    size_t dgram_len = sizeof(tcphdr);
    ssize_t bytes_sent = sendto(sock, segment, segment_size, 0, (struct sockaddr*)&addr, sizeof(addr));

    if (bytes_sent < 0)
        err_sys("[send_dgram]error sending packet");
    
    printf("[send_dgram]Sending datagram to UTCP port %u, UDP port %u\r\n", tcb->fourtuple.dest_port, tcb->dest_udp_port);
    print_tcphdr(&segment->hdr);
    
    free(segment);
    tcb->snd_nxt += len;
    return bytes_sent;
}

ssize_t rcv_dgram(int sock, uint8_t rcvbuf[1024], struct sockaddr_in* from)
{
    /**
     * @brief receive a datagram
     */
    //struct sockaddr_storage from;
    ssize_t rcvsize;
    ssize_t buflen = 1500;
    socklen_t fromlen = sizeof(*from);
    rcvsize = recvfrom(sock, rcvbuf, buflen, 0, (struct sockaddr *)from, &fromlen); // # bytes rcv'd

    if (rcvsize < 0)
        err_sys("[rcv_dgram]Failed to receive datagram");
    return rcvsize;
}

void update_fsm(int utcp_fd, enum conn_state state)
{ 
    /**
     * @brief update the FSM state of a given UTCP socket
     */
    struct tcb *tcb = get_tcb(utcp_fd);
    if (tcb == NULL)
        err_sys("[update_fsm]Invalid UTCP socket");
    
    tcb->fsm_state = state;
}

void err_sys(const char* x)
{
    /**
     * @brief A global error logging function.
     * 
     * @example err_sys("bind"); -> "bind: Address already in use"
     */
    perror(x);
    exit(EXIT_FAILURE);
}

void err_sock(int sock, const char* x)
{
    /**
     * @brief Closes a problematic socket, then prints an error message.
     * 
     * If an error occurs during the closing process, that message prints first.
     */
    if (close(sock) == -1)
        err_sys("(err_sock)socket close failed:");
    err_sys(x);
}