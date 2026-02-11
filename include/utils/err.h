#ifndef ERR_H
#define ERR_H

#include <stdint.h>

/* Begin function declaration */

/**
 * @brief A global error logging function.
 * 
 * @example err_sys("bind"); -> "bind: Address already in use"
 */
void err_sys(const char* x);

/**
 * @brief Closes a problematic socket, prints an error message, and exits
 * 
 * @note If an error occurs during the closing process, that message prints first.
 */
void err_sock(int sock, const char* x);

/**
 * @brief Called when a fatal error related
 * to data can occur
 */
void err_data(const char* x);

/* End function declaration*/

#endif
