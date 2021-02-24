#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <stdbool.h>
#include "commands.h"


bool is_file_command(char *msg) {
    if (strncmp(msg, "FILE: ", 6) == 0) {
        return true;
    }
    return false;
}

int endswith(const char *str, const char *suffix) {
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}



// serverside
bool handle_command(char *msg, int src) {

    if (is_file_command(msg)) {
        printf("Trying file..\n");
        file_transfer(msg, src);
        printf("Complete\n");
        return true;
    }
    return false;
}


// sending client
bool client_command(char *msg, int dst) {
    int len = strlen(msg);
    char ack[len];
    
    if (is_file_command(msg)) {
        pause_recv = true;

        write(dst, msg, len);
        read(dst, ack, len);

        if (strncmp("ACK", ack, 4)) {
            printf("Invalid ACK : %s", ack);
            exit(1);
        }
        send_though_server(msg, dst);

        pause_recv = false;
        return true;
    }
    return false;
}


// receiving client
bool incoming_command(char *msg, int src) {
    char *p;

    if (is_file_command(msg)) {
        char path[FILEPATH_LEN];
        int idx = 0;
        p = msg+6;
        
        while (p[++idx] != ' ');
        strncpy(path, p, idx);


        recv_file(src, path);
        printf("got it!\n");
        return true;
    }
    return false;
}


// route though server
int file_transfer(char *msg, int src) {
    char data[FILE_BUF], *p = msg+6;
    int dst, bytes_recv, bytes_sent, break_signal = 0;
    int len = strlen(msg);
    
    // get dst sock
    while (*p++ != ' ');
    dst = atoi(p);

    // pass SYN ACK   
    write(dst, msg, len);
    read(dst, data, 4);  
    data[4] = 0;
    write(src, data, 4);

    printf("here\n");
    while (1) {
        bytes_recv = read(src, data, FILE_BUF);
        p = data;
        
        if(endswith(data, END)) {
            break_signal = 1;
        }

        while (bytes_recv > 0) {
            bytes_sent = write(dst, p, bytes_recv);
            bytes_recv -= bytes_sent;
            p += bytes_sent;
        }
        if (break_signal) {
            break;
        }
        bzero(data, FILE_BUF);
    }
}


// sending client
int send_though_server(char *msg, int sock) {
    int fd, idx = 0;
    char path[FILEPATH_LEN], *p = msg+6;

    while (p[++idx] != ' ');
    strncpy(path, p, idx);
    
    if ((fd = open(path, O_RDONLY)) < 1){
        printf("filepath %s not recognised\n", path);
        return -1;
    }
    printf("Sending file\n");
    send_file(sock, fd);
    return idx;
}





int recv_file(int sock, char *filepath) {
    char data[FILE_BUF];
    int bytes_read, bytes_written, fd, break_signal = 0;
    unsigned char *p;

    if ((fd = open(filepath, O_WRONLY)) < 0) {
        printf("Failed to open file %s for writing\n",
                                    filepath);
        return -1;
    }

    write(sock, "ACK", strlen("ACK"));

    printf("incoming\n");
    while (1) { 
        bytes_read = read(sock, data, FILE_BUF);
        p = data;

        if (endswith(data, END)) {
            bytes_read -= strlen(END);
            break_signal = 1;
        }

        while (bytes_read > 0) {
            bytes_written = write(fd, p, bytes_read);
            bytes_read -= bytes_written;
            p += bytes_written;
        }
        if (break_signal){break;}
    }
    return fd;
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
    write(target, END, sizeof END);
    printf("Sent!\n");
    return 0;
}
