#ifndef PRINTABLE_H
#define PRINTABLE_H

#include <stdbool.h>

#include <tcp/tcphdr.h>
#include <tcp/tcb.h>
#include <tcp/fourtuple.h>

/* Begin function declaration */
bool is_null(const void *ptr, const char *msg);
void print_tcphdr(const tcphdr *tcp);
void print_tcb(const tcb *tcb);
void print_fourtup(const fourtuple *tup);
/* End function declaration*/
#endif
