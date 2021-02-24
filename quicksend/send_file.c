#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>

#define MAX_BYTES  200
#define FILE_BUF   1024


void exit_procedure(int sock, struct addrinfo *info, int exitcode);
void *recv_thread(void * client);
void send_connect(int sock);
int recv_file(int sock, char *filepath);
int send_file(int target, int fd);


int main(int argc, const char **argv){
    char port[] = "3490"; 
    int status, sock;
    struct addrinfo hints;
    struct addrinfo *clinfo;

    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if (argc < 3) {
        printf("%s : usage %s [address [port]] [filepath]\n",
                argv[0], argv[0]);
        exit(1);
    }
    if (argc > 3) {
        strcpy(port, argv[1]);
    }
    if ((status = getaddrinfo(argv[1],   // if not passive req adr
                              port,
                              &hints,
                              &clinfo)) != 0){
        printf("Error getting info : %s\n", gai_strerror(status));
        exit_procedure(0, clinfo, 1);
    }
    if ((sock = socket(clinfo->ai_family,
                       clinfo->ai_socktype,
                       clinfo->ai_protocol)) == -1){
        printf("Error creating sock : %s\n", port);
        exit_procedure(sock, clinfo, 2);
    }
    if (connect(sock, 
                clinfo->ai_addr, 
                clinfo->ai_addrlen) == -1) {
        printf("Error connecting to : %s", port);
        exit_procedure(sock, clinfo, 3);
    }
    printf("sending file : %s on port : %s\n",
           (argc > 3) ? argv[3] : argv[2], port);
    int fp = open((argc > 3) ? argv[3] : argv[2], O_RDONLY);
    send_file(sock, fp);
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

