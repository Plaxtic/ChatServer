#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "quicksend.h"


void exit_procedure(int sock, struct addrinfo *info) {
    freeaddrinfo(info);
    close(sock);
    return;
}


int recv_file(int sock, char *filepath) {
    char data[FILE_BUF];
    int bytes_read, bytes_written, fp;
    void *p;

    if ((fp = open(filepath, O_WRONLY)) < 0) {
        FILE *create = fopen(filepath, "w");
        fclose(create);
        
        if ((fp = open(filepath, O_WRONLY)) < 0) {
            printf("Failed to open file %s for writing\n",
                                                  filepath);
            return -1;
        }
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

int send_file_from_sock(char *filepath, char *ip, char *port) {
    int status, sock, fd;
    struct addrinfo hints;
    struct addrinfo *clinfo;

    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    printf("%s : |%s|\n", __FUNCTION__, ip);
    if ((status = getaddrinfo(ip,   // if not passive req adr
                              port,
                              &hints,
                              &clinfo)) != 0){
        printf("Error getting info : |%s|\n", gai_strerror(status));
        return 1;
    }
    if ((sock = socket(clinfo->ai_family,
                       clinfo->ai_socktype,
                       clinfo->ai_protocol)) == -1){
        printf("Error creating sock : %s\n", port);
        exit_procedure(sock, clinfo);
        return 2;
    }
    if (connect(sock, 
                clinfo->ai_addr, 
                clinfo->ai_addrlen) == -1) {
        printf("Error connecting to : %s", port);
        exit_procedure(sock, clinfo);
        return 3;
    }
    if ((fd = open(filepath, O_RDONLY)) <= 0) {
        printf("Error opening file %s\n", filepath);
        exit_procedure(sock, clinfo);
        return 4;
    }
    printf("Sending file : %s on port : %s\n",
           filepath, port);
    send_file(sock, fd);
    printf("File transfer complete!\n");
    exit_procedure(sock, clinfo);
    return 0;
}



int recv_file_on_sock(char *filepath, char *ip, char *port){
    int status, sock, client; 
    struct sockaddr_storage client_info;
    struct addrinfo hints;
    struct addrinfo *servfo;
    socklen_t addr_size;

    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if ((status = getaddrinfo(ip,
                              port,
                              &hints,
                              &servfo)) != 0) {
        printf("Error getting info : %s\n", gai_strerror(status));
        return 1;
    }
    if ((sock = socket(servfo->ai_family,
                       servfo->ai_socktype,
                       servfo->ai_protocol)) == -1) {
        printf("Error creating sock : %s\n", port);
        exit_procedure(sock, servfo);
        return 2;
    }
    if (bind(sock, servfo->ai_addr, servfo->ai_addrlen) == -1) {
        printf("Failed to bind port : %s\n", port);
        exit_procedure(sock, servfo);
        return 3;
    }
    if (listen(sock, QUE) == -1) {
        printf("Listening failed on : %s\n", port);
        exit_procedure(sock, servfo);
        return 4;
    }
    addr_size = sizeof client_info;
    client = accept(sock,
                    (struct sockaddr *)&client_info,
                    &addr_size);

    printf("Incoming file : %s\n", filepath);
    recv_file(client, filepath);
    printf("File arrived!\n");
    exit_procedure(sock, servfo);
    return 0;
}

