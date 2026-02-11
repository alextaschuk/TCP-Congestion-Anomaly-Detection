#ifndef PRINTABLE_H
#define PRINTABLE_H

#include <stdbool.h>
#include <netinet/tcp.h>

#include <tcp/tcb.h>
#include <tcp/fourtuple.h>

/* Begin function declaration */

bool is_null(const void *ptr, const char *msg);

/**
 * @brief print the contents of a
 * TCP header that is in network
 * byte order
 */
void print_tcphdr(const struct tcphdr *tcp);


void print_tcb(const tcb_t *tcb);


void print_fourtuple(const fourtuple *tup);

/* End function declaration */
#endif
