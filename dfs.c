/**************************************
* This is the distributed filesystem
* server. 
*
* Ben Heberlein
**************************************/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define RECSIZE 1024

void error(char *msg) {
    perror(msg);
    exit(1);
}

void warn(char *msg) {
    perror(msg);
}

void process(int sock) {

    char rec[RECSIZE];
    int num = 0;
    
    /* Get request data */
    num = read(sock, rec, RECSIZE-1);

    if (num < 0) {
        error("read");
    }

    printf("Received the following\n%s\n",rec);
}

int main(int argc, char *argv[]) {

    uint16_t port = 10100;

    /* Get port number */
    if (argc != 2) {
        printf("Please pass in a port number\n");
        exit(1);
    }
    port = atoi(argv[1]);

    /* Start server */
    printf("Starting server with port %d\n", port);
    struct sockaddr_in serverAddr;
    struct sockaddr_in clientAddr;
    int sock = 0;
    int opt = 0;
    int tempSock = 0;
    int clientLen = 0;
    int thread = 0;

    /* Initialize socket */
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(-1);
    }

    /* Clear socket structure */
    memset(&serverAddr, 0, sizeof(serverAddr));
    memset(&clientAddr, 0, sizeof(clientAddr));
    clientLen = sizeof(clientAddr);

    /* Allow quick release of socket port */
    opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void *) &opt, sizeof(int));

    /* Set socket address, port, etc */
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Bind host address */
    if (bind(sock, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
        perror("bind");
        exit(-1);
    }

    /* Listen for incoming connections */
    if (listen(sock, 10)) {
        perror("listen");
        exit(-1);
    }

    /* Start connection loop */
    while (1) {

        /* Accept new connection */
        tempSock = accept(sock, (struct sockaddr *) &clientAddr, &clientLen);

        /* Make sure accept worked */
        if (tempSock < 0) {
            perror("accept");
            exit(-1);
        }

        /* Create new thread to handle connection */
        thread = fork();

        /* Check if fork worked */
        if (thread < 0) {
            perror("fork");
            exit(-1);
        }

        /* Call to fork returns 0 in child thread and thread ID in parent thread */
        if (thread == 0) {
            printf("Running worker process\n");

            /* Only need client socket from accept */
            close(sock);

            /* Process request and terminate thread */
            process(tempSock);
            exit(0);

        } else {
            printf("Started worker thread with ID %d\n", thread);

            /* Continue listening for other connections */
            close(tempSock);
        }
    }

    return 0;


}
