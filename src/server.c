#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


int main(void) {
    struct sockaddr_in sa = {
        // Configure and define a socket to receive data (IP + port #)

        .sin_family = AF_INET, // use IPv4 address; use AF_INET6 for IPv6

        /*
         Most devices store bytes in big endian, but
         some are in little-endian. All data sent over internet
         protocols is sent in big-endian, so htonl (host to
         network long) takes the data you want to send and
         converts it to network byte order (ensures that data
         is always sent as big endian).
        */
        .sin_addr.s_addr = htonl(INADDR_ANY), // 4 byte IP address
        
        // htons converts the unsigned short integer from host byte
        // order to network byte order.
        .sin_port = htons(4444) // port number
    };
    

    char buffer[1024];
    ssize_t recsize;
    socklen_t fromlen = sizeof(sa);

    /*
    int bind(int sockfd, struct sockaddr *my_addr, int addrlen);
     - sockfd: socket file descriptor returned by socket()
     - my_addr: pointer to sockaddr struct (info about a socket)
     - addrlen: length (in bytes) of socket addr
    */
    int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (bind(sock, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
        fprintf(stderr, "Failed to bind socket!\n");
        close(sock);
        return EXIT_FAILURE;
    }

    while (1) {
        recsize = recvfrom(sock, (void*)buffer, sizeof buffer, 0, (struct sockaddr*)&sa, &fromlen);
        if (recsize < 0) {
            fprintf(stderr, "%s\n", strerror(errno));
            return EXIT_FAILURE;
        }
        printf("recsize: %d\n ", (int)recsize);
        sleep(1);
        printf("datagram: %.*s\n", (int)recsize, buffer);
    }
}
