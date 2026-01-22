#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>

int main(void) {
    struct addrinfo hints, *res;
    int sockfd;
    struct sockaddr_storage from;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // accept either IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // fill in my IP addr for me

    getaddrinfo(NULL, "7654", &hints, &res);

    //make a socket (sockfd = socket file descriptor)
    // sockfd is an int that refers to the socket obj in the kernel
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    
    char buffer[1024];
    ssize_t recsize; // the number of bytes received, -1 on error
    //socklen_t fromlen = sizeof(sockfd); // length of socket

    // prevent "address already in use" message when trying to rerun the server
    int yes = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    //bind it to the port passed in getaddrinfo
    //int bind(int sockfd, struct sockaddr *my_addr, int addrlen);
     // addrlen: length (in bytes) of socket addr
     // `->` used to access a field of a struct via a pointer
    if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1){
        fprintf(stderr, "Failed to bind socket!\n");
        close(sockfd);
        return EXIT_FAILURE;
    }

    while (1) {
        socklen_t fromlen = sizeof from;
        // int recvfrom(int sockfd, void *buf, int len, uint flags, struct sockaddr *from, int *fromlen);
        recsize = recvfrom(sockfd, buffer, sizeof buffer, 0, (struct sockaddr *)&from, &fromlen);

        if (recsize < 0) {
            fprintf(stderr, "%s\n", strerror(errno));
            return EXIT_FAILURE;
        }

        printf("recsize: %d\n", (int)recsize);
        printf("datagram: %.*s\n", (int)recsize, buffer);
    }
}
