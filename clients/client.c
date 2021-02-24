#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "../commands/commands.h"

#define MAX_BYTES 200
static bool pause_thread = false;

void exit_procedure(int sock, struct addrinfo *info, int exitcode);
void * recv_thread(void * client);
void send_connect(int sock);

int main(int argc, const char **argv){
    char port[] = "4390";
    int status, sock;
    struct addrinfo hints;
    struct addrinfo *clinfo;
    pthread_t rThread;

    if (argc < 2) {
        printf("client usage: ./client <address>\n");
        exit(1);
    }
    if (argc > 2) {
        strcpy(port, argv[2]);
    }
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_INET;    // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets

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
        printf("Error connecting to : %s\n", port);
        exit_procedure(sock, clinfo, 3);
    }
    printf("Connected to: %s on sock %d\n", argv[1], sock);
    pthread_create(&rThread, 
                         NULL, 
                         recv_thread, 
                         (void *) &sock);

    send_connect(sock);
    exit_procedure(sock, clinfo, 0);
}


void * recv_thread(void * client) {
    char msg[MAX_BYTES];
    int bytes;
    int *cl = (int *)client; 

    while ((bytes = recv(*cl, msg, MAX_BYTES, 0)) > 0) {
        while(pause_recv);
        msg[bytes] = '\0';

        if (!incoming_command(msg, *cl)) {
            printf("%s", msg);
        }
    }
    return client;
}


void send_connect(int sock) {
    char out_msg[MAX_BYTES];

    while (strcmp(out_msg, "q\n")) {
        while(pause_send);
        int len, bytes_sent;
        fgets(out_msg, MAX_BYTES, stdin);
        len = strlen(out_msg);

        if (client_command(out_msg, sock)) {
            continue;
        }
        if ((bytes_sent = send(sock, out_msg, len, 0)) == -1
                || bytes_sent < len) {
            printf("Msg send failed : %d of %d\n",
                    bytes_sent,
                    len);
        }
    }
}


void exit_procedure(int sock, struct addrinfo *info, int exitcode) {
    freeaddrinfo(info);
    close(sock);
    exit(exitcode);
}
