#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <ev.h>

#define HELPSTRING "Usage: cdemo -p [port]\n"
#define SA struct sockaddr

long getport(int argc, char *argv[]) {
    if (argc < 3) {
        printf(HELPSTRING);
        exit(0);
    }

    if (strcmp(argv[1], "-p") == 0) {
        const char *nptr = argv[1];
        char *endptr = NULL;
        errno = 0;
        long port =  strtol(argv[2], &endptr, 10);
        if (nptr == endptr)
            errno = 1;
        else if (errno == 0 && nptr && *endptr != 0)
            errno = 1;

        if (errno != 0 || port <= 0) {
            printf("Invalid port value\n");
            exit(0);
        }
        return port;
    } else {
        printf(HELPSTRING);
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    long port = getport(argc, argv);

    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("Socket creation failed\n");
        exit(0);
    }
    else
        printf("Socket created\n");
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    char *s = inet_ntoa(servaddr.sin_addr);
    printf("IP address: %s\n", s);
    printf("Port: %li \n", port);

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        printf("Socket bind failed\n");
        exit(0);
    }
    else
        printf("Socket successfully bound\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0) {
        printf("Listen failed\n");
        exit(0);
    }
    else
        printf("Server listening\n");
    len = sizeof(cli);

    // Accept the data packet from client and verification
    connfd = accept(sockfd, (SA*)&cli, &len);
    if (connfd < 0) {
        printf("server accept failed...\n");
        exit(0);
    }
    else
        printf("server accept the client...\n");

    // Function for chatting between client and server
    //func(connfd);

    // After chatting close the socket
    close(sockfd);

    return 0;
}


