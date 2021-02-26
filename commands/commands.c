#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdbool.h>
#include <pthread.h>
#include "commands.h"
#include "../quicksend/quicksend.h"


typedef struct _arg_struct {
    char args[MAX_ARGS][FILEPATH_LEN];
} arg_struct;

                 /*  HELPERS  */
///////////////////////////////////////////////////////////
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

void path_parse(char *path, char *msg) {
    char *p = msg;

    // seek wordend
    while (*++msg);

    // seek filename begining
    while (*--msg != '/' && msg >= p);
    strcpy(path, ++msg);
}

void arg_parse(char args[MAX_ARGS][FILEPATH_LEN], char *msg, int num_args) {

    for (int i = 0; i < num_args; ++i) {
        int idx = 0;

        // index wordend
        while (msg[idx] != ' ' && msg[idx] != '\n' && msg[idx]) {
            idx++;
        }
        if (idx > FILEPATH_LEN) {
            printf("Argument %d too long\n", i+1);
        }
        strncpy(args[i], msg, idx);

        args[i][idx] = 0;

        msg += idx;

        // skip spaces
        while (*msg == ' ') {msg++;};

    }
}


int ip_lookup(char *ip, char ip_table[FD_RANGE][INET6_ADDRSTRLEN], int max_fd) {
    int fd;
    for (fd = 0; fd < max_fd; ++fd) {
        if (strncmp(ip, ip_table[fd], strlen(ip)) == 0) {
            printf("IP match : %s, %s\n", ip, ip_table[fd]);
            return fd;
        }
    }
    return -1;
}

bool handshake(int dst, int src, char *msg) {
    char ack[3];

    printf("Initiating handshake\n");

    write(dst, msg, strlen(msg));
    read(dst, ack, 3);
    if (strcmp(ack, "ACK")) {
        write(src, "RST", 3);
        return false;
    }
    write(src, ack, 3);

    if (read(src, ack, 3) <= 0) {
        return false;
    }
    if (strcmp(ack, "ACK")) {
        return false;
    }
    return true;
}

void * file_recv_thread(void * wrapped) {
     arg_struct *args = (arg_struct *)wrapped;

     printf("IP after cast : %s\n", args->args[2]);

     printf("args:\n1 = %s\n2 = %s\n3 = %s\n4 = %s\n", 
             args->args[0],
             args->args[1],
             args->args[2],
             args->args[3]);

     recv_file_on_sock(args->args[1], args->args[2], "3490");
     return wrapped;
}

void * file_send_thread(void * wrapped) {
     printf("deref : %s\n", *((char **)wrapped));
     char args[MAX_ARGS][FILEPATH_LEN];

     arg_parse(args, *((char **)wrapped), 3);
     // arg_struct *args = (arg_struct *)wrapped; 

     printf("IP after cast : %s\n", args[2]);

     printf("args:\n1 = %s\n2 = %s\n3 = %s\n4 = %s\n", 
             args[0],
             args[1],
             args[2],
             args[3]);

     send_file_from_sock(args[1], args[2], "3490");
     return wrapped;
}
///////////////////////////////////////////////////////



// serverside
bool handle_command(int src, char *msg, int max_fd, 
                    char ip_table[FD_RANGE][INET6_ADDRSTRLEN]) {
    char args[MAX_ARGS][FILEPATH_LEN];

    if (is_file_command(msg)) {
        int dst;

        arg_parse(args, msg, 3);
        
        if ((dst = ip_lookup(args[2], ip_table, max_fd)) == -1) {
            printf("Invalid IP adr : %s\n", args[2]);
        }
        if (!handshake(dst, src, msg)) {
            printf("Error : File transfer %s to %s failed\n", 
                                           args[1], args[2]);
        }
        printf("Handshake sucsessfull\n");
        return true;
    }
    return false;
}
            

// sending client
bool client_command(char *msg, int server) {
    char args[MAX_ARGS][FILEPATH_LEN];
    pthread_t thread;
    char ack[3];
    

    if (is_file_command(msg)){
        pause_recv = true;
        
        arg_parse(args, msg, 3);
        
        printf("parsed, waiting..\n");
        write(server, msg, strlen(msg));
        read(server, ack, 3);
        
        printf("Received ACK = %s\n", ack);
        if (strncmp(ack, "ACK", 3)) {
            printf("Invalid ACK : %s\n", ack);
            return true;
        }
        write(server, "ACK", 3);

        printf("IP before cast : %s\n", args[2]);

        pthread_create(&thread, NULL, 
                       file_send_thread, 
                       (void *)&args);
        return true;
    }
    return false;
}


// receiving client
bool incoming_command(char *msg, int src) {
    char args[MAX_ARGS][FILEPATH_LEN];
    pthread_t thread2;

    if (is_file_command(msg)) {
        
        arg_parse(args, msg, 3);

        printf("parsed\n");

        printf("IP before cast : %s\n", args[2]);

        pthread_create(&thread2, NULL, 
                       file_recv_thread, 
                       (void *)&args);
        write(src, "ACK", 3);
        return true;
    }
    return false;
}
/////////////////////////////////////////////////////









// route though server
int file_transfer(char *msg, int src) {
    char data[FILE_BUF] = {0};
    char *p = msg+6;
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
    char path[FILEPATH_LEN] = {0};
    char *p = msg+6;

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




/*
int recv_file(int sock, char *filepath) {
    char data[FILE_BUF] = {0};
    char command[strlen(filepath)+strlen("touch ")];
    int bytes_read, bytes_written, fd, break_signal = 0;
    unsigned char *p;

    if ((fd = open(filepath, O_WRONLY)) < 0) {
        snprintf(command, sizeof command, "touch %s", filepath);
        system(command);
        if ((fd = open(filepath, O_WRONLY)) < 0) {
            printf("Failed to open file %s for writing\n",
                                         filepath);
            return -1;
        }
    }

    write(sock, "ACK", strlen("ACK"));

    printf("Incoming file %s\n", filepath);
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
}*/
