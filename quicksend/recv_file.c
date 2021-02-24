#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <fcntl.h>

#define QUE        10
#define MAX_BYTES  200
#define FILE_BUF   1024


void exit_procedure(int sock, struct addrinfo *info, int exitcode);
int recv_buf(int client, int msg_size, char msg[MAX_BYTES]);
void * recv_thread(void * client);
int send_connect(int sock);
int recv_file(int sock, char *filepath);
int send_file(int target, int fd);


int main(int argc, char **argv){
    char port[6] = "3490";
    char ip[16];
    int status, sock, client, ret;
    struct sockaddr_storage client_info;
    struct addrinfo hints;
    struct addrinfo *servfo;
    socklen_t addr_size;
    pthread_t rThread;

    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (argc < 3) {
        printf("%s : usage %s [address [port]] [filepath]\n", 
                argv[0], argv[0]);
        exit(1);
    }
    if (argc > 3) {
        strcpy(port, argv[2]);
    }        
    if ((status = getaddrinfo(argv[1],
                              port,
                              &hints,
                              &servfo)) != 0) {
        printf("Error getting info : %s\n", gai_strerror(status));
        exit(2);
    }
    if ((sock = socket(servfo->ai_family,
                       servfo->ai_socktype,
                       servfo->ai_protocol)) == -1) {
        printf("Error creating sock : %s\n", port);
        exit_procedure(sock, servfo, 3);
    }
    if (bind(sock, servfo->ai_addr, servfo->ai_addrlen) == -1) {
        printf("Failed to bind port : %s\n", port);
        exit_procedure(sock, servfo, 4);
    }
    if (listen(sock, QUE) == -1) {
        printf("Listening failed on : %s\n", port);
        exit_procedure(sock, servfo, 5);
    }
    addr_size = sizeof client_info;

    client = accept(sock,
                    (struct sockaddr *)&client_info,
                    &addr_size);

    recv_file(client, (argc > 3) ? argv[3] : argv[2]);
    exit_procedure(sock, servfo, 0);
}


void exit_procedure(int sock, struct addrinfo *info, int exitcode) {
    freeaddrinfo(info);
    close(sock);
    exit(exitcode);
}


int recv_file(int sock, char *filepath) {
    char data[FILE_BUF];
    int bytes_read, bytes_written, fp;
    void *p;

    if ((fp = open(filepath, O_WRONLY)) < 0) {
        printf("Failed to open file %s for writing\n",
                                    filepath);
        return -1;
    }
    while ((bytes_read = read(sock, data, FILE_BUF)) > 0) {
        p = data;

        while (bytes_read > 0) {
            bytes_written = write(fp, p, bytes_read);
            bytes_read -= bytes_written;
            p += bytes_written;
        }
    }
    return fp;
}


int send_file(int target, int fd) {
    char data[FILE_BUF] = {0};
    int bytes_read, bytes_written;
    void *p;

    while ((bytes_read = read(fd, data, FILE_BUF)) > 0) {
        p = data;

        while (bytes_read > 0) {
            bytes_written = write(target, p, bytes_read);
            bytes_read -= bytes_written;
            p += bytes_read;
        }
    }
    return 0;
}

