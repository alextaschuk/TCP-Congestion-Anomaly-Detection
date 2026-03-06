#ifndef PRINTABLE_H
#define PRINTABLE_H

#include <stdbool.h>

#include <netinet/tcp.h>

#include <tcp/fourtuple.h>
#include <tcp/hndshk_fsm.h>
#include <tcp/tcb.h>


/* Begin function declaration */

/**
 * @brief helper function to determine if the struct to be printed is `NULL` or not.
 * 
 * @param *ptr A pointer of any type.
 * @param *msg A string to print if `*ptr` is indeed `NULL`.
 * 
 * @returns `true` if `*ptr` is `NULL`, `false` otherwise.
 */
static bool is_null(const void *ptr, const char *msg);

/**
 * @brief Prints a TCP segment's header and payload (if there is one) in a readable format.
 * 
 * @param *buf A TCP segment. This is a buffer of data that is received via `recvfrom` or sent via `sendto`.
 * @param buflen The length of the segment, in bytes.
 * @param flow Defines the segment's direction. If the segment is being sent (outgoing) this should be `0`.
 *  If the packet is being received (incoming) this should be `1`.
 * 
 * @note The segment that is passed when this function is called should be in NETWORK order.
 */
void print_segment(const uint8_t *buf, const size_t buflen, const bool flow);


/**
 * Prints a TCB in a readable format.
 * 
 * @param *tcb The TCB to print.
 */
void print_tcb(const tcb_t *tcb);


/**
 * Prints a four tuple that consists of:
 * 
 * - A source UTCP port
 * 
 * - A source IP address
 * 
 * - A destination UTCP port
 * 
 * - A destination IP address
 * 
 * @param *tup The four tuple to print.
 */
void print_fourtuple(const fourtuple_t *tup);


/**
 * Prints the data in TCP segment's payload that is printable up to the first 64 printable characters (the rest are truncated).
 */
void print_safe_chars(uint8_t *buf, size_t len);


/**
 * A nice helper function to print a connection's current state (e.g., `ESTABLISHED`).
 */
const char* fsm_state_to_str(enum conn_state state);


/**
 * Prints all TCBs in the global lookup table.
 */
void print_lookup(void);

/* End function declaration */
#endif
