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
#include <dirent.h>

#define LIST  0
#define GET   1
#define PUT   2
#define MKDIR 3

#define SUCCESS     0
#define CREDENTIALS 255

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

typedef struct rsp_s {
    uint32_t func;
    uint32_t err;
    uint32_t dlen;
    //uint32_t chunk;
    //uint32_t filename;
    //uint32_t directory;
    char data[256];
} rsp_t;

void error(char *msg) {
    perror(msg);
    exit(1);
}

void warn(char *msg) {
    perror(msg);
}

void get(msg_t rec, int sock) {
    int num = 0;
    char *rbuf;
    char path[256] = "";
    char ch[4] = "";
    char *temp;
    char line[256];
    int user_flag = 0;
    FILE *f;
    FILE *config;
    rsp_t rsp;
    int len = 0;

    printf("Got a packet\n");

    /* Check username and password */
    config = fopen("dfs.conf", "r");
    user_flag = 0;
    while(fgets(line, sizeof(line), config)) {
        temp = strtok(line, " \r\n");
        if (strcmp(temp, rec.username) == 0) {
            printf("Found matching username\n");
            temp = strtok(NULL, " \r\n");
            if (strcmp(temp, rec.password) == 0) {
                printf("Found matching password\n");
                user_flag = 1;
                break;
            }
        }
    }
    fclose(config);
    if (user_flag == 0) {
        printf("Username and password not matched\n");
        printf("Username: %s\n", rec.username);
        printf("Password: %s\n", rec.password);

        /* Build response and send */
        rsp.func = GET;
        rsp.err = CREDENTIALS;
        strcpy(rsp.data, "Invalid username/password. Please try again.");
        send(sock, &rsp, sizeof(rsp), 0);

        return;
    }

    /* Construct path */
    strcpy(path, server_name);
    mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    strcat(path, rec.username);
    strcat(path, rec.directory);
    //printf("Creating dir at %s\n", path);
    mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    strcat(path, ".");
    strcat(path, rec.filename);
    strcat(path, ".");
    sprintf(ch, "%d", rec.chunk);
    strcat(path, ch);

    /* Open file */
    f = fopen(path, "r");
    if (f == NULL) {
        printf("Couldn't open file");
        return;
    }

    /* Get file len */
    fseek(f, 0L, SEEK_END);
    len = ftell(f);
    fseek(f, 0L, SEEK_SET);

    /* Open buffer for chunk */
    rbuf = (char *) malloc(len + 32);

    /* Get read data and save */
    num = fread(rbuf, 1, len, f);
    printf("Data size: %d %d\n", num, len);
    if (num != len) {
        printf("Could not read file data\n");
    }

    /* Send response packet */
    rsp.func = GET;
    rsp.err = SUCCESS;
    rsp.dlen = len;
    strcpy(rsp.data, "Successfully read file chunk\n");
    send(sock, &rsp, sizeof(rsp), 0);

    /* Send data */
    send(sock, rbuf, len, 0);
    printf("Sent data chunk\n");

    /* Close file */
    fclose(f);

    /* Free memory */
    free(rbuf); 

}

void put(msg_t rec, int sock) {
    int num = 0;
    char *rbuf;
    char path[256] = "";
    char ch[4] = "";
    char *temp;
    char line[256];
    int user_flag = 0;
    FILE *f;
    FILE *config;
    rsp_t rsp;

    /* Check username and password */
    config = fopen("dfs.conf", "r");
    user_flag = 0;
    while(fgets(line, sizeof(line), config)) {
        temp = strtok(line, " \r\n");
        if (strcmp(temp, rec.username) == 0) {
            printf("Found matching username\n");
            temp = strtok(NULL, " \r\n");
            if (strcmp(temp, rec.password) == 0) {
                printf("Found matching password\n");
                user_flag = 1;
                break;
            }
        }
    }
    fclose(config);
    if (user_flag == 0) {
        printf("Username and password not matched\n");
        printf("Username: %s\n", rec.username);
        printf("Password: %s\n", rec.password);
        
        /* Build response and send */
        rsp.func = PUT;
        rsp.err = CREDENTIALS;
        strcpy(rsp.data, "Invalid username/password. Please try again.");
        send(sock, &rsp, sizeof(rsp), 0);
        
        return;
    }

    /* Open buffer for chunk */
    rbuf = (char *) malloc(rec.dlen + 32);

    /* Construct path */
    strcpy(path, server_name);
    mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    strcat(path, rec.username);
    strcat(path, rec.directory);
    //printf("Creating dir at %s\n", path);
    mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    strcat(path, ".");
    strcat(path, rec.filename);
    strcat(path, ".");
    sprintf(ch, "%d", rec.chunk);
    strcat(path, ch);

    /* Open first file */
    printf("Opening file %s\n", path);
    f = fopen(path, "w");
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

    /* Get request data for second packet */
    printf("Before pkt %d %d %d %s %s %s %s\n", rec.func, rec.chunk, rec.dlen, rec.username, rec.password, rec.filename, rec.directory);
    num = read(sock, &rec, sizeof(rec));
    if (num != sizeof(rec)) {
        error("read");
    }
    printf("After pkt %d %d %d %s %s %s %s\n", rec.func, rec.chunk, rec.dlen, rec.username, rec.password, rec.filename, rec.directory);

    /* Check username and password */
    config = fopen("dfs.conf", "r");
    user_flag = 0;
    while(fgets(line, sizeof(line), config)) {
        temp = strtok(line, " \r\n");
        if (strcmp(temp, rec.username) == 0) {
            printf("Found matching username\n");
            temp = strtok(NULL, " \r\n");
            if (strcmp(temp, rec.password) == 0) {
                printf("Found matching password\n");
                user_flag = 1;
                break;
            }
        }
    }
    fclose(config);
    if (user_flag == 0) {
        printf("Username and password not matched\n");
        printf("Username: %s\n", rec.username);
        printf("Password: %s\n", rec.password);
     
        /* Build response and send */
        rsp.func = PUT;
        rsp.err = CREDENTIALS;
        strcpy(rsp.data, "Invalid username/password. Please try again.");
        send(sock, &rsp, sizeof(rsp), 0);

        return;
    }

    /* Construct path */
    strcpy(path, server_name);
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

    /* Open second file */
    printf("Opening file %s\n", path);
    f = fopen(path, "w");
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

    /* Send response */
    rsp.func = PUT;
    rsp.err = SUCCESS;
    strcpy(rsp.data, "Successfully saved files to ");
    strcat(rsp.data, server_name);
    send(sock, &rsp, sizeof(rsp), 0);

}

void list(msg_t rec, int sock) {
    char path[256];
    char temp_path[256];
    char line[256];
    char *temp;
    char ch[4];
    FILE *config;
    FILE *f;
    rsp_t rsp;
    int user_flag = 0;

    /* Check username and password */
    config = fopen("dfs.conf", "r");
    user_flag = 0;
    while(fgets(line, sizeof(line), config)) {
        temp = strtok(line, " \r\n");
        if (strcmp(temp, rec.username) == 0) {
            printf("Found matching username\n");
            temp = strtok(NULL, " \r\n");
            if (strcmp(temp, rec.password) == 0) {
                printf("Found matching password\n");
                user_flag = 1;
                break;
            }
        }
    }
    fclose(config);
    if (user_flag == 0) {
        printf("Username and password not matched\n");
        printf("Username: %s\n", rec.username);
        printf("Password: %s\n", rec.password);

        /* Build response and send */
        rsp.func = LIST;
        rsp.err = CREDENTIALS;
        strcpy(rsp.data, "Invalid username/password. Please try again.");
        send(sock, &rsp, sizeof(rsp), 0);

        return;
    }

    /* Construct path */
    strcpy(path, server_name);
    mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    strcat(path, rec.username);
    //strcat(path, rec.directory);
    //strcat(path, ".");
    //strcat(path, rec.filename);
    //strcat(path, ".");
    printf("Looking for files at %s/*\n", path);

    /* Get all files */
    DIR *d;
    struct dirent *dir;
    d = opendir(path);
    if (d == NULL) {
        printf("Server has no files\n");
        return;
    }
    while((dir = readdir(d)) != NULL) {
        printf("%s\n", dir->d_name);
        if (strcmp(dir->d_name, ".") == 0 ||
            strcmp(dir->d_name, "..") == 0) {
            continue;
        }
        rsp.func = LIST;
        rsp.err = SUCCESS;
        strcpy(rsp.data, "");
        strcat(rsp.data, path);
        strcat(rsp.data, "/");
        strcat(rsp.data, dir->d_name);
        send(sock, &rsp, sizeof(rsp), 0); 
    }
    printf("Scanned all files\n");
    closedir(d);

}

void makedir(msg_t rec) {

}

void process(int sock) {
    msg_t rec;
    int num = 0;
    char *rbuf;
    char path[256] = "";
    char ch[4] = "";
    char *temp;
    char line[265];    
    int user_flag = 0;
    FILE *f;
    FILE *config;

    /* Get request data */
    num = read(sock, &rec, sizeof(rec));
    if (num != sizeof(rec)) {
        return;
    }

    /* Put request */
    if (rec.func == PUT) {
        put(rec, sock);
    } else if (rec.func == GET) {
        get(rec, sock);
    } else if (rec.func == LIST) {
        list(rec, sock);
    } else if (rec.func == MKDIR) {
        makedir(rec);
    } else {
        printf("Received invalid function\n");
        return;
    }


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
    if (strstr(server_name, "/") == NULL) {
        strcat(server_name, "/");
    }
    
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
            //printf("Running worker process\n");

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
