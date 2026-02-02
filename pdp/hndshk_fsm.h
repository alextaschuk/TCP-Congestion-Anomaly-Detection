/**
 * @brief This finite state machine tracks the current connection
 * status for a client/server. A connection is maintained by
 * including a user's PDP datagram in the payload field of a UDP
 * datagram.
 * 
 * The connection status follows the same state machine used in
 * in TCP for establishing a persistent connection via a 3-Way
 * Handshake, and terminating a connection via a 4-Way Handshake.
 * 
 * See https://en.wikipedia.org/wiki/Transmission_Control_Protocol#Protocol_operation,
 *     http://tcpipguide.com/free/t_TCPOperationalOverviewandtheTCPFiniteStateMachineF-2.htm
 *     https://upload.wikimedia.org/wikipedia/commons/a/a2/Tcp_state_diagram_fixed.svg
 * 
 * Acronyms:
  * SYN -- synchronize
  * ACK -- acknowledge
  * FIN -- finish (connection termination request)
  * rcv/rcv'd -- receive/received
  * pkts -- packets
 */
#ifndef HNDSHK_FSM_H
#define HNDSHK_FSM_H

enum
{
  // [endpoint], description
  LISTEN = 1, /*[server], waiting (listening) for a connection*/
  SYN_SENT, /*[client], waiting for SYN-ACK from server*/
  SYN_RECEIVED,/*[server], awaiting ACK from client for its SYN-ACK*/
  ESTABLISHED, /*[client, server], connection is established; data can flow*/
  FIN_WAIT_1, /*[client, server], local app is waiting for ACK for its FIN or is waiting for a FIN from remote*/
  FIN_WAIT_2, /*[client, server], local app rcv'd ACK for its FIN, waiting for FIN from remote*/
  CLOSE_WAIT, /*[client, server], remote app has sent FIN, local app's socket is still open*/
  CLOSING, /*[client, server], rcv'd FIN from remote & sent ACK, hasn't rcv'd ACK from remote for its own FIN*/
  LAST_ACK, /*[client, server], local rcv'd FIN & sent ACK, waiting for ACK from remote for its own FIN*/
  TIME_WAIT, /*[client OR server], wait to ensure all remaining pkts on the connection have expired*/
  CLOSED /*[client, server], no connection state (can describe state before 3-way hndshk and after 4-way hndshk)*/
};
#endif
