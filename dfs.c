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
#include <sys/stat.h>

#define RECSIZE 1024

// TODO read conf file for username and password
char username[64] = "Alice";
char password[64] = "SimplePassword";

// TODO pass in server ID as arg
char server_name[64] = "DFS1";

typedef struct msg_s {
    uint32_t func;
    uint32_t chunk;
    uint32_t dlen;
    char username[64];
    char password[64];
    char filename[64];
    char directory[64];
} msg_t;

void error(char *msg) {
    perror(msg);
    exit(1);
}

void warn(char *msg) {
    perror(msg);
}

void process(int sock) {
    msg_t rec;
    int num = 0;
    char *rbuf;
    char path[256] = "";
    char ch[4] = "";
    
    /* Get request data */
    num = read(sock, &rec, sizeof(rec));

    if (num != sizeof(rec)) {
        error("read");
    }

    /* Check username and password */
    if (strcmp(username, rec.username) != 0) {
        printf("Bad username");
    }
    if (strcmp(password, rec.password) != 0) {
        printf("Bad password");
    }
 
    /* Open buffer for chunk */
    rbuf = (char *) malloc(rec.dlen + 32);

    /* Construct path */
    strcat(path, server_name);
    strcat(path, "/");
    mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    strcat(path, rec.username);
    strcat(path, rec.directory);
    printf("Creating dir at %s\n", path);
    mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    strcat(path, ".");
    strcat(path, rec.filename);
    strcat(path, ".");
    sprintf(ch, "%d", rec.chunk);
    strcat(path, ch);

    /* Open first file */
    printf("Opening file %s\n", path);
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        printf("Could not open file\n");
        return;
    }
    printf("Opened file\n");

    /* Get read data and save */
    num = read(sock, rbuf, rec.dlen);
    printf("Data size: %d %d\n", num, rec.dlen);
    if (num != rec.dlen) {
        printf("Could not read file data\n");
    }
    fwrite(rbuf, 1, num, f); 
    printf("Wrote file\n");

    /* Close file */
    fclose(f);
   
    /* Free memory */
    free(rbuf);
}

int main(int argc, char *argv[]) {

    uint16_t port = 10100;

    /* Get port number */
    if (argc != 3) {
        printf("Please pass in a server name and a port number\n");
        exit(1);
    }
    port = atoi(argv[2]);
    strcpy(server_name, argv[1]);

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

    /* Set socket recieve timeout (200ms) */
/*    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        error("Error setting socket timeout");
    } */

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
            //perror("accept");
            //exit(-1);
            continue;
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
