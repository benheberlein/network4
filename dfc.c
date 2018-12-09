/*************************************
* This is the distributed filesystem
* client. 
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
#include <openssl/md5.h>

#define LIST  0
#define GET   1
#define PUT   2
#define MKDIR 3

int dfs_sock[4];
struct sockaddr_in dfs_addr[4];
char username[64];
char password[64];

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

void list() {

}

void get(char *filename) {

}

void put(char *filename) {
    FILE *f = fopen(filename, "r");
    MD5_CTX m;
    int num = 0;
    char temp[512];
    unsigned char md5_buf[MD5_DIGEST_LENGTH];
    int md5_mod = 0;
    int ret = 0;
    msg_t pkt;
    int len = 0;
    int left = 0;
    char *rbuf;
    
    /* Open file */
    if (f == NULL) {
        printf("Invalid file\n");
        return;
    }

    printf("Opened file\n");
    
    /* Get file len/4 */
    fseek(f, 0L, SEEK_END);
    len = ftell(f) / 4;
    // TODO leftover / 4
    fseek(f, 0L, SEEK_SET);
    if (len % 4 == 0) {
        left = len;
    }

    /* Compute MD5 hash */
    MD5_Init(&m);
    num = fread(temp, 1, 512, f);
    while (num > 0) {
        MD5_Update(&m, temp, num);
        num = fread(temp, 1, 512, f);
    }
    MD5_Final(md5_buf, &m);

    printf("MD5 hash is ");
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        printf("%02x", md5_buf[i]);
    }
    printf("\n");
    
    /* Get modulo */
    md5_mod = md5_buf[MD5_DIGEST_LENGTH-1] % 4;
    printf("Modulo is %d\n", md5_mod);

    /* Create file buffer */
    rbuf = (char *) malloc(len);

    /* Start on index associated with modulo */
    for (int i = 0; i < 4; i++) {
        
        printf("Sending to server %d", md5_mod);

        /* Build first packet */
        pkt.func  = GET;
        pkt.chunk = i;
        pkt.dlen = len;
        strcpy(pkt.username, username);
        strcpy(pkt.password, password);
        strcpy(pkt.filename, filename);
        strcpy(pkt.directory, "\\");

        /* Send packet */
        ret = send(dfs_sock[md5_mod], &pkt, sizeof(pkt), 0);
        if (ret < 0) {
            printf("Packet failed\n");
        }

        /* Send first data chunk */
        fseek(f, len * i, SEEK_SET);
        num = fread(rbuf, 1, len, f);
        ret = send(dfs_sock[md5_mod], rbuf, len, 0);

        /* Build second packet */
        pkt.chunk = (i + 1) % 4;
 
        /* Send packet */
        ret = send(dfs_sock[md5_mod], &pkt, sizeof(pkt), 0);
        if (ret < 0) {
            printf("Packet failed\n");
        }

        /* Send second data chunk */
        fseek(f, len * ((i + 1) % 4), SEEK_SET);
        num = fread(rbuf, 1, len, f);
        ret = send(dfs_sock[md5_mod], rbuf, len, 0);

        /* Go to next server index */
        md5_mod = (md5_mod + 1) % 4;
    }

}

void mkdir(char *dirname) {

}

int main(int argc, char *argv[]) {
    char dfs[4][64];
    char dfs_host[4][32];
    char dfs_port[4][8];

    char *temp;
    char line[256];
    int dfs_index = 0;
    char user_input[64];
    char *user_oper; 
    char *user_arg;
    int serv_len = 0;
    int ret = 0;

    /* Parse file for server names, user, and password */
    FILE *config = fopen("dfc.conf", "r");
    if (config == NULL) {
        error("Could not open config file");
    }
    while(fgets(line, sizeof(line), config)) {
        //printf("%s\n", line);
        if (strstr(line, "Server DFS") != NULL) {
            printf("%d\n", dfs_index);
            temp = strtok(line, " \r\n");
            temp = strtok(NULL, " \r\n");
            temp = strtok(NULL, " \r\n");
            printf("%s\n", temp);
            strcpy(dfs[dfs_index], temp);
            temp = strtok(dfs[dfs_index], ":"); 
            //printf("%s\n", temp);
            strcpy(dfs_host[dfs_index], temp);
            printf("%s\n", dfs_host[dfs_index]);
            temp = strtok(NULL, ":");
            //printf("%s\n", temp);
            strcpy(dfs_port[dfs_index], temp);
            printf("%s\n", dfs_port[dfs_index]);
            dfs_index++;
        }
        if (strstr(line, "Username") != NULL) {
            temp = strtok(line, " \r\n");
            temp = strtok(NULL, " \r\n");
            printf("Username: %s\n", temp);
            strcpy(username, temp);
        }
        if (strstr(line, "Password") != NULL) {
            temp = strtok(line, " \r\n");
            temp = strtok(NULL, " \r\n");
            printf("Password: %s\n", temp);
            strcpy(password, temp);
        }
    }

    /* Create sockets */
    for (int i = 0; i < 1; i++ ){
        printf("Creating socket %d\n", i);
        dfs_sock[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (dfs_sock < 0) {
            error("Error inializing socket");
        }

        /* Build server address */
        bzero((char *) &dfs_addr[i], sizeof(dfs_addr[i]));
        dfs_addr[i].sin_family = AF_INET;
        dfs_addr[i].sin_port = htons(atoi(dfs_port[i]));
        if (inet_pton(AF_INET, dfs_host[i], &dfs_addr[i].sin_addr) <= 0) {
            error("Invalid host address for server");
        }

        if (connect(dfs_sock[i], (struct sockaddr *) &dfs_addr[i], sizeof(dfs_addr[i])) < 0) {
            error("Connection failed");
        }

        printf("Sending to socket %d\n", i);
        serv_len = sizeof(dfs_addr[i]);
        ret = send(dfs_sock[i], "hello world", 11, 0);
        if (ret < 0) {
            printf("Packet failed\n");
        }
    }

    /* Command loop */
    while(1) {
        fgets(user_input, 64, stdin);

        /* Prevent empty input */
        if (strcmp("\n", user_input) == 0) {
            printf("Invalid option. Options are:\n\tlist\n\tget\n\tput\n\tmkdir\n");
            continue;
        }
   
        /* Strip off first word */
        user_oper = strtok(user_input, " \n\r\t");

        /* Select operation */
        if (strcmp("list", user_oper) == 0) {
            printf("Sending list command\n");
            list();
        } else if(strcmp ("get", user_oper) == 0) {
            user_arg = strtok(NULL, " \n\t\r");
            if (user_arg == NULL) {
                printf("Needs an argument for file to get\n");
                continue;
            }
            printf("Sending get command\n");
            get(user_arg);
        } else if(strcmp ("put", user_oper) == 0) {
            user_arg = strtok(NULL, " \n\t\r");
            if (user_arg == NULL) {
                printf("Needs an argument for file to put\n");
                continue;
            }
            printf("Sending put command\n");
            put(user_arg);
         } else if(strcmp ("mkdir", user_oper) == 0) {
            user_arg = strtok(NULL, " \n\t\r");
            if (user_arg == NULL) {
                printf("Needs an argument for directory name\n");
                continue;
            }
            printf("Sending mkdir command\n");
            mkdir(user_arg);
        } else {
            printf("Invalid option. Options are:\n\tlist\n\tget\n\tput\n\tmkdir\n");
            continue;
        }
    }
}