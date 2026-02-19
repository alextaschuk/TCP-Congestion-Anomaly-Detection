#ifndef CLIENT_H
#define CLIENT_H

#include <netinet/in.h>

#include <tcp/tcb.h>

/*Define variables*/

extern int client_fsm;
extern int sock;
extern int UTCP_sock;
extern struct sockaddr_in client_addr;
extern struct sockaddr_in server_addr;

/*End define variables*/

/* Define functions */

/**
 * @brief Initiates a 3-way handshake by sending a
 * SYN packet, receiving and handling a SYN-ACK packet,
 * and responding with an ACK packet.
 */
static void perform_hndshk(const int sock, const int utcp_fd);

/**
 * @brief allows us to enter the server's port
 * number if we don't hardcode the value.
 */
static void set_server_port(void);

/**
 * @brief sends out a SYN packet and updates
 * the sender's TCB FSM state and `syn_nxt` value.
 * @param sock the UDP socket to send the datagram out of
 * @param utcp_fd UTCP socket's position in tcb_lookup
 */
static void send_syn(int sock, int utcp_fd, tcb_t *tcb);

/**
 * @brief receive and handle the SYN-ACK packet that is sent
 * in response to the callee's SYN packet.
 */
static void rcv_syn_ack(int sock, int utcp_fd, tcb_t *tcb);


/**
 * @brief send an ACK packet in response to the
 * SYN-ACK packet that the callee received.
 */
static void send_ack(int sock, int utcp_fd);

/* End define functions */
#endif
