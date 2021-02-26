#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include "quicksend.h"


void exit_procedure(int sock, struct addrinfo *info) {
    freeaddrinfo(info);
    close(sock);
    return;
}

char memformat(char *memrep, float bytes) {
    char sufix[] = "BKMGT";

    for (int i = 0; i < 4; ++i) {
        if (!floor(bytes /= 1024)) {
            sprintf(memrep, "%.2f%c", bytes*1024, sufix[i]);
            return sufix[i];
        }
    } return 0;
}



int progress_bar(float size, float sent, char *bar) {
    int i;

    int percent = floor((sent/size) * 100);
    int dashes  = floor(percent/2);
    
    for (i = 0; i < dashes; ++i) {
        bar[i] = '-';
    }
    bar[i] = '>';
    return percent;
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


int send_file(int target, int fd, float size) {
    char human_r[10];
    int bytes_read, bytes_written, total, percent, max_p;
    void *p;
    char data[FILE_BUF] = {0};
    char bar[51];
    memset(bar, ' ', 51);

    time_t start = time(NULL); 

    total = percent = max_p = 0;
    memformat(human_r, size);

    while ((bytes_read = read(fd, data, FILE_BUF)) > 0) {
        total += bytes_read;
        p = data;
        
        while (bytes_read > 0) {
            bytes_written = write(target, p, bytes_read);
            bytes_read -= bytes_written;
            p += bytes_read;
        }
        if ((percent = progress_bar(size, total, bar))/2 > max_p/2) {
            max_p = percent;

            int secs = time(NULL) - start; 
            printf("[%s] %3d%% of %s | %02d:%02d\n",
                    bar, percent, 
                    human_r, secs/60, secs%60);
        }
    }
    return 0;
}


int send_file_from_sock(char *filepath, char *ip, char *port) {
    int status, sock, fd;
    long unsigned size;
    struct addrinfo hints;
    struct addrinfo *clinfo;
    int yes=1;

    FILE *fp = fopen(filepath, "r");
    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);
    fclose(fp);

    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    
    if ((status = getaddrinfo(ip,
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
    send_file(sock, fd, size);
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
    int yes=1;

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
    if (setsockopt(sock, SOL_SOCKET,SO_REUSEADDR, 
                            &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(1);
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

