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


struct arg_struct {
    char *ip;
    char *path;
};

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

void arg_parse(char args[NARGS][PATHLEN], char *msg, int num_args) {

    for (int i = 0; i < num_args; ++i) {
        int idx = 0;

        // index wordend
        while (msg[idx] != ' ' && msg[idx] != '\n' && msg[idx]) {
            idx++;
        }
        if (idx > PATHLEN) {
            printf("Argument %d too long\n", i+1);
        }
        
        //copy till wordend
        strncpy(args[i], msg, idx);
        args[i][idx] = 0;
    
        // jump to wordend
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
    
    // forward command to dst
    write(dst, msg, strlen(msg));

    // get and return response
    read(dst, ack, 3);
    write(src, ack, 3);

    if (read(src, ack, 3) <= 0) {
        return false;
    }
    if (strcmp(ack, ACK)) {
        return false;
    }
    return true;
}

void * file_recv_thread(void * wrapped) {
    char path[PATHLEN];
    
    // cut path from filename
    path_parse(path, ((struct arg_struct *)wrapped)->path);

    printf("IP after cast : %s\n", ((struct arg_struct *)wrapped)->ip);
    recv_file_on_sock(path, ((struct arg_struct *)wrapped)->ip, "3490");
    return wrapped;
}

void * file_send_thread(void * wrapped) {

    printf("IP after cast : %s\n", ((struct arg_struct *)wrapped)->ip);
    send_file_from_sock(((struct arg_struct *)wrapped)->path, 
                         ((struct arg_struct *)wrapped)->ip, "3490");
    return wrapped;
}
///////////////////////////////////////////////////////



// serverside
bool handle_command(int src, char *msg, int max_fd, 
                    char ip_table[FD_RANGE][INET6_ADDRSTRLEN]) {
    char args[NARGS][PATHLEN];

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
    char args[NARGS][PATHLEN];
    char ack[3];
    struct arg_struct arg_s;
    pthread_t thread;
    

    if (is_file_command(msg)){

        // pause recv thread 
        pause_recv = true;
        
        // get args 
        arg_parse(args, msg, 3);
        
        // send command to sever
        write(server, msg, strlen(msg));

        // recv and check ACK
        read(server, ack, 3);
        printf("Received ACK = %s\n", ack);
        if (strncmp(ack, ACK, 3)) {
            printf("Invalid ACK : %s\n", ack);
            write(server, "RST", 3);
            return true;
        }
        // end server handshake
        write(server, ACK, 3);

        // pass args to struct (for thread)
        arg_s.path = args[1];
        arg_s.ip   = args[2];

        printf("IP before cast : %s\n", arg_s.ip);

        // start thread
        int ret = pthread_create(&thread, NULL, 
                       file_send_thread, 
                       (void *)&arg_s);

        // keep function live so struct isn't erased
        sleep(5);
        return true;
    }
    return false;
}


// receiving client
bool incoming_command(char *msg, int src) {
    struct arg_struct arg_s;
    char args[NARGS][PATHLEN];
    pthread_t thread;

    if (is_file_command(msg)) {
        
        // get args
        arg_parse(args, msg, 3);
        
        // pass to struct
        arg_s.path = args[1];
        arg_s.ip   = args[2];

        printf("IP before cast : %s\n", arg_s.ip);
        
        // create
        pthread_create(&thread, NULL, 
                       file_recv_thread, 
                       (void *)&arg_s);
        write(src, ACK, 3);
        sleep(5);
        return true;
    }
    return false;
}
/////////////////////////////////////////////////////
// OLD
////////////////////////////////////////////////////








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
    char path[PATHLEN] = {0};
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

