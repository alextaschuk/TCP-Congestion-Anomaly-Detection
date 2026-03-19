#ifndef CONN_H
#define CONN_H
/*
Logic related to connection establishment and management,
whether that be for a UDP socket, a UTCP socket, or anything
else.
*/

#include <stdint.h>

#include <netinet/in.h>

#include <tcp/tcb.h>

/** 
 * @brief A application-side function that is used to establish a UTCP connection.
 * 
 * This function creates a new TCB for the client and initializes its necessary values.
 * 
 * @return The FD of the newly-created TCB.
 */
int utcp_connect(int utcp_fd, const struct sockaddr_in *dest_addr);

/**
 * @brief An application-side function to bind a UDP socket.
 * 
 * After binding a socket to the given port, the socket's fd
 * is stored in our global `api_t` struct.
 * 
 * @param pts (Port to Set), the port that the app wants to bind a UDP socket to passes in the 
 * - If we hardcode the port, this value is not needed.
 * 
 * @returns The bound port. 
 * 
 * - If a hardcoded value (i.e., anything other than 0 is passed in), this value will be returned.
 * 
 */
uint16_t bind_udp_sock(int pts);

/**
 * @brief We need to manually manage a `bind()` function
 * since client sockets need to be bound according to
 * their UTCP port number, not the actual UDP port number.
 * 
 * @param addr contains socket's source IP address and port number
 * 
 * @return `int fd` on success, the UTCP socket's "file descriptor". I.e.,
 *  the newly created `tcb_t` struct is stored at `tcb_lookup[fd]`.
 */
int utcp_bind(int utcp_fd, struct sockaddr_in *addr);


int utcp_sock(void);



/**
 * @brief Used in `utcp_accept()` function when a client wants to initiate a 3WHS.
 * 
 * This function is identical to `utcp_bind()`, except for 3 things:
 * 
 * 1. It doesn't set the newly-allocated TCB's source UTCP port or source IP. 
 * 
 * 2. It saves the TCB's FD inside of itself (`new_tcb->fd = fd;`).
 * 
 * 3. It returns the new TCB struct instead of its FD.
 */
tcb_t *alloc_new_tcb(void);


/**
 * Searches the global lookup table for the server's listen TCB.
 * 
 * @return A pointer to the listen TCB's struct, or `NULL` if it isn't found.
 */
tcb_t *find_listen_tcb(void);


void *utcp_listen_thread(void *arg);


/**
 * @brief An application-side function to tell us (pretending we're the OS)
 * that the app is ready to receive connection requests.
 * 
 * The SYN and accept queues are initialized and the listen socket's state is
 * set to `LISTEN`.
 * @param utcp_fd The listen socket's TCB FD
 * @param backlog The maximum number of TCBs that each queue can store at a time
 */
int utcp_listen(api_t *global, int backlog);


/**
 * @brief An application-side function that is called when the app wants to accept
 * a connection request that is sitting in the accept queue.
 * 
 * @returns `-1` for invalid socket, or `int fd`, a UTCP fd, on success.
 */
int utcp_accept(api_t *global);


/**
 * Spawns the listen thread and ticker thread.
 * @return `-1` on error, `0` on success.
 */
int spawn_threads(api_t *global);

#endif