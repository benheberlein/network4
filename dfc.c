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

#define SUCCESS     0
#define CREDENTIALS 255

#define MAX_FILES 100

int dfs_sock[4];
struct sockaddr_in dfs_addr[4];
char dfs[4][64];
char dfs_host[4][32];
char dfs_port[4][8];

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

typedef struct rsp_s {
    uint32_t func;
    uint32_t err;
    uint32_t dlen;
    //uint32_t chunk;
    //uint32_t filename;
    //uint32_t directory;
    char data[256];
} rsp_t;

typedef struct clist_s {
    char name[64];
    uint8_t c0;
    uint8_t c1;
    uint8_t c2;
    uint8_t c3;
} clist_t;

void create_socks();
void close_socks();

void error(char *msg) {
    perror(msg);
    exit(1);
}

void warn(char *msg) {
    perror(msg);
}

void list() {
    int num = 0;
    int ret = 0;
    char file_list[4096] = "";
    char temp_file[256];
    char temp_code[8];
    char *temp;
    msg_t pkt;
    rsp_t rsp;
    clist_t clist[MAX_FILES];
    int curr_chunk = 0;
    int len = 0;
    int index = 0;
    char *rbuf;
    FILE *f;

    /* List first to see what files server has */
    pkt.func = LIST;
    pkt.chunk = 0;
    pkt.dlen = 0;
    strcpy(pkt.username, username);
    strcpy(pkt.password, password);
    strcpy(pkt.filename, "");
    strcpy(pkt.directory, "/");

    /* Send to all servers */
    for (int i = 0; i < 4; i++) {

        /* Send packet */
        ret = send(dfs_sock[i], &pkt, sizeof(pkt), 0);
        if (ret < 0) {
            printf("Packet failed\n");
        }

        /* Get response from servers */
        //for (int j = 0; j < 2; j++) {
        while(1) {
            num = read(dfs_sock[i], &rsp, sizeof(rsp));
            if (num < 0) {
                printf("Timed out waiting for server DFS%d\n", i +  1);
                break;
            } else if (rsp.func == LIST && rsp.err == CREDENTIALS) {
                printf("DFS%d: %s\n", i + 1, rsp.data);
            } else if (rsp.func == LIST && rsp.err == SUCCESS) {
                //printf("DFS%d: %s\n", i + 1, rsp.data);

                /* Parse file name and get chunk index */
                temp = strtok(rsp.data, "/");
                temp = strtok(NULL, "/");
                temp = strtok(NULL, "/");
                if (temp == NULL) {
                    break;
                }

                /* Get index */
                len = strlen(temp);
                index = (int) temp[len-1] - 48;

                /* Save only filename */
                len = strlen(temp);
                temp[len-2] = '\0';
                strcpy(temp_file, temp+1);

                /* Insert into list of files */
                len = strlen(temp);
                temp = strstr(file_list, temp_file);
                if (temp != NULL) {
                    *(temp+len+index) = (char) (i + 48); 
                    //printf("%s\n", file_list);
                } else {
                    strcat(file_list, temp_file);
                    strcat(file_list, " ZZZZ ");
                    temp = strstr(file_list, temp_file);
                    *(temp+len+index) = (char) (i + 48); 
                    //printf("%s\n", file_list);
                }
                
            } else {
                printf("Invalid message from server\n");    
            }
        }
    }

    /* Go through all files in list and print */
    temp = strtok(file_list, " ");
    printf("Files on DFS:\n");
    while(temp != NULL) {
        printf("%s", temp);
        temp = strtok(NULL, " ");
        if (strstr(temp, "Z")) {
            printf(" [incomplete]");
        }
        printf("\n");
        temp = strtok(NULL, " ");
    }
 
}

void get(char *filename) {
    int num = 0;
    int ret = 0;
    char file_list[4096] = "";
    char temp_file[256];
    char temp_code[8];
    char *temp;
    msg_t pkt;
    rsp_t rsp;
    clist_t clist[MAX_FILES];
    int curr_chunk = 0;
    int len = 0;
    int index = 0;
    char *rbuf;
    FILE *f;

    /* List first to see what files server has */
    pkt.func = LIST;
    pkt.chunk = 0;
    pkt.dlen = 0;
    strcpy(pkt.username, username);
    strcpy(pkt.password, password);
    strcpy(pkt.filename, filename);
    strcpy(pkt.directory, "/");

    /* Send to all servers */
    for (int i = 0; i < 4; i++) {

        /* Send packet */
        ret = send(dfs_sock[i], &pkt, sizeof(pkt), 0);
        if (ret < 0) {
            printf("Packet failed\n");
        }

        //usleep(1000);

        /* Get response from servers */
        //for (int j = 0; j < 2; j++) {
        while(1) {
            num = read(dfs_sock[i], &rsp, sizeof(rsp));
            if (num < 0) {
                printf("Timed out waiting for server DFS%d\n", i +  1);
                break;
            } else if (rsp.func == LIST && rsp.err == CREDENTIALS) {
                printf("DFS%d: %s\n", i + 1, rsp.data);
            } else if (rsp.func == LIST && rsp.err == SUCCESS) {
                //printf("DFS%d: %s\n", i + 1, rsp.data);

                /* Parse file name and get chunk index */
                temp = strtok(rsp.data, "/");
                temp = strtok(NULL, "/");
                temp = strtok(NULL, "/");
                if (temp == NULL) {
                    break;
                }

                /* Get index */
                len = strlen(temp);
                index = (int) temp[len-1] - 48;

                /* Save only filename */
                len = strlen(temp);
                temp[len-2] = '\0';
                strcpy(temp_file, temp+1);

                /* Insert into list of files */
                len = strlen(temp);
                temp = strstr(file_list, temp_file);
                if (temp != NULL) {
                    *(temp+len+index) = (char) (i + 48); 
                    //printf("%s\n", file_list);
                } else {
                    strcat(file_list, temp_file);
                    strcat(file_list, " ZZZZ ");
                    temp = strstr(file_list, temp_file);
                    *(temp+len+index) = (char) (i + 48); 
                    //printf("%s\n", file_list);
                }
                
            } else {
                printf("Invalid message from server\n");    
            }
        }
    }

    /* See if file is in list */
    temp = strstr(file_list, filename);
    if (temp != NULL) { 
        temp = strtok(temp, " ");
        printf("Found file in list, %s\n", temp);
        strcpy(temp_file, temp);
        temp = strtok(NULL, " ");
        printf("Code %s\n", temp);
        strcpy(temp_code, temp);
        if (strstr(temp_code, "Z") == NULL) {
            printf("Found complete file across four servers\n");
            
            /* Open file */
            f = fopen(filename, "w");
            if (f == NULL) {
                printf("Could not open file on client\n");
                return;
            }

            /* Make request for each chunk */
            for (int i = 0; i < 4; i++) {
                printf("Making request for chunk %d to DFS%d\n", i, (temp_code[i]-48) + 1);

                /* Reopen sockets */
                close_socks();
                create_socks();
            
                /* Build packet */
                pkt.func  = GET;
                pkt.chunk = i;
                pkt.dlen = 0;
                strcpy(pkt.username, username);
                strcpy(pkt.password, password);
                strcpy(pkt.filename, filename);
                strcpy(pkt.directory, "/");

                /* Send packet */
                ret = send(dfs_sock[temp_code[i]-48], &pkt, sizeof(pkt), 0);
                if (ret < 0) {
                    printf("Packet failed\n");
                }

                /* Wait for response */
                num = read(dfs_sock[temp_code[i]-48], &rsp, sizeof(rsp));
                if (num < 0) {
                    printf("Could not read chunk %d from DFS%d\n", i, (temp_code[i]-48) + 1);
                    continue;
                } else if (rsp.func == GET && rsp.err == CREDENTIALS) {
                    printf("DFS%d: %s\n", (temp_code[i]-48) + 1, rsp.data);
                    continue;
                } else if (rsp.func == GET && rsp.err == SUCCESS) {
                    printf("DFS%d: %s\n", (temp_code[i]-48) + 1, rsp.data);
                } else {
                    printf("Invalid message from server\n");
                    continue;
                }

                /* Wait for data */
                rbuf = (char *) malloc(rsp.dlen+32); 
                num = read(dfs_sock[temp_code[i]-48], rbuf, rsp.dlen);
                printf("Received chunk %d\n", i);
                //printf("%s", rbuf);
                //printf("\n");
                                
                /* Save to file */
                fwrite(rbuf, 1, rsp.dlen, f);

                /* Free memory */
                free(rbuf);
            }

            /* Close file */
            fclose(f);
            printf("Successfully wrote %s\n", filename);

        } else {
            printf("File is incomplete. Missing file chunks\n");
            return;
        }
    } else {
        printf("Could not find file on server\n");
        return;
    }

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
    char *rbuf;
    rsp_t rsp;
    
    /* Open file */
    if (f == NULL) {
        printf("Invalid file\n");
        return;
    }
    printf("Opened file\n");
    
    /* Get file len/4 */
    fseek(f, 0L, SEEK_END);
    len = (ftell(f) + (4 - 1)) / 4;
    fseek(f, 0L, SEEK_SET);

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
        
        printf("Sending to DFS%d\n", md5_mod + 1);

        /* Read chunk */
        fseek(f, len * i, SEEK_SET);
        num = fread(rbuf, 1, len, f);

        /* Build first packet */
        pkt.func  = PUT;
        pkt.chunk = i;
        pkt.dlen = num;
        strcpy(pkt.username, username);
        strcpy(pkt.password, password);
        strcpy(pkt.filename, filename);
        strcpy(pkt.directory, "/");

        /* Send packet */
        ret = send(dfs_sock[md5_mod], &pkt, sizeof(pkt), 0);
        if (ret < 0) {
            printf("Packet failed\n");
        }

        /* Send first data chunk */
        ret = send(dfs_sock[md5_mod], rbuf, num, 0);

        /* Read chunk */
        fseek(f, len * ((i + 1) % 4), SEEK_SET);
        num = fread(rbuf, 1, len, f);

        /* Build second packet */
        pkt.chunk = (i + 1) % 4;
        pkt.dlen  = num;
 
        /* Send packet */
        ret = send(dfs_sock[md5_mod], &pkt, sizeof(pkt), 0);
        if (ret < 0) {
            printf("Packet failed\n");
        }

        /* Send second data chunk */
        ret = send(dfs_sock[md5_mod], rbuf, num, 0);

        /* Get response from servers */
        num = read(dfs_sock[md5_mod], &rsp, sizeof(rsp));
        if (num < 0) {
            printf("Could not save data chunks %d and %d to DFS%d\n", i, (i + 1) % 4, md5_mod +  1);
        } else if (rsp.func == PUT && rsp.err == CREDENTIALS) {
            printf("DFS%d: %s\n", md5_mod + 1, rsp.data);
        } else if (rsp.func == PUT && rsp.err == SUCCESS) {
            printf("DFS%d: %s\n", md5_mod + 1, rsp.data);
        } else {
            printf("Invalid message from server\n");    
        }
        
        /* Go to next server index */
        md5_mod = (md5_mod + 1) % 4;

    }

    /* Free memory */
    free(rbuf);
}

void makedir(char *dirname) {

}

void create_socks() {
    /* Create sockets */
    struct timeval tv;
    for (int i = 0; i < 4; i++ ){
        //printf("Creating socket %d\n", i);
        dfs_sock[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (dfs_sock < 0) {
            error("Error inializing socket");
        }

        /* Set socket recieve timeout (200ms) */
        tv.tv_sec = 0;
        tv.tv_usec = 200000;
        if (setsockopt(dfs_sock[i], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            error("Error setting socket timeout");
        }

        /* Build server address */
        bzero((char *) &dfs_addr[i], sizeof(dfs_addr[i]));
        dfs_addr[i].sin_family = AF_INET;
        dfs_addr[i].sin_port = htons(atoi(dfs_port[i]));
        if (inet_pton(AF_INET, dfs_host[i], &dfs_addr[i].sin_addr) <= 0) {
            error("Invalid host address for server");
        }

        if (connect(dfs_sock[i], (struct sockaddr *) &dfs_addr[i], sizeof(dfs_addr[i])) < 0) {
            //error("Connection failed");
            printf("Connection failed\n");
        }
    }
}

void close_socks() {
    for (int i = 0; i < 4; i++) {
        close(dfs_sock[i]);
    }
}

int main(int argc, char *argv[]) {
    //char dfs[4][64];
    //char dfs_host[4][32];
    //char dfs_port[4][8];

    char *temp;
    char line[256];
    int dfs_index = 0;
    char user_input[64];
    char *user_oper; 
    char *user_arg;
    int serv_len = 0;
    int ret = 0;

    if (argc != 2) {
        printf("Not enough arguments. please supply a file\n");
        exit(1);
    }

    /* Parse file for server names, user, and password */
    FILE *config = fopen(argv[1], "r");
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
#if 0
    struct timeval tv;
    for (int i = 0; i < 4; i++ ){
        printf("Creating socket %d\n", i);
        dfs_sock[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (dfs_sock < 0) {
            error("Error inializing socket");
        } 

        /* Set socket recieve timeout (200ms) */
        tv.tv_sec = 0;
        tv.tv_usec = 200000;
        if (setsockopt(dfs_sock[i], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            error("Error setting socket timeout");
        }

        /* Build server address */
        bzero((char *) &dfs_addr[i], sizeof(dfs_addr[i]));
        dfs_addr[i].sin_family = AF_INET;
        dfs_addr[i].sin_port = htons(atoi(dfs_port[i]));
        if (inet_pton(AF_INET, dfs_host[i], &dfs_addr[i].sin_addr) <= 0) {
            error("Invalid host address for server");
        }

        if (connect(dfs_sock[i], (struct sockaddr *) &dfs_addr[i], sizeof(dfs_addr[i])) < 0) {
            //error("Connection failed");
            printf("Connection failed\n");
        }
    }
#endif

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
            create_socks();
            list();
            close_socks();
        } else if(strcmp ("get", user_oper) == 0) {
            user_arg = strtok(NULL, " \n\t\r");
            if (user_arg == NULL) {
                printf("Needs an argument for file to get\n");
                continue;
            }
            printf("Sending get command\n");
            create_socks();
            get(user_arg);
            close_socks();
        } else if(strcmp ("put", user_oper) == 0) {
            user_arg = strtok(NULL, " \n\t\r");
            if (user_arg == NULL) {
                printf("Needs an argument for file to put\n");
                continue;
            }
            printf("Sending put command\n");
            create_socks();
            put(user_arg);
            close_socks();
         } else if(strcmp ("mkdir", user_oper) == 0) {
            user_arg = strtok(NULL, " \n\t\r");
            if (user_arg == NULL) {
                printf("Needs an argument for directory name\n");
                continue;
            }
            printf("Sending mkdir command\n");
            create_socks();
            makedir(user_arg);
            close_socks();
        } else {
            printf("Invalid option. Options are:\n\tlist\n\tget\n\tput\n\tmkdir\n");
            continue;
        }
    }
}
