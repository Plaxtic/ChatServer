#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <stdbool.h>
#include "commands/commands.h"

#define PORT "4390"
#define FD_RANGE 20
#define FILE_BUF 1024

void *get_in_addr(struct sockaddr *sa);
int get_listener_socket(char *ip_address);
void add_to_pfds(struct pollfd *pfds[], 
                 int newfd, int *fd_count, int *fd_size);
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count); 
void send_online(int new_sock, int fd_count, struct pollfd *pfds);
int fd_from_adr(char *adr, struct pollfd *pfds, int fd_count,
                char addresses[FD_RANGE][INET6_ADDRSTRLEN]);
// Main
int main(int argc, const char **argv) {
    int listener;     // Listening socket descriptor
    int newfd;        // Newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // Client address
    socklen_t addrlen;
    char buf[256];    // Buffer for client data
    char msg[MSG_LEN];
    char remoteIP[INET6_ADDRSTRLEN];
    char addresses[FD_RANGE][INET6_ADDRSTRLEN];
    // Start off with room for 5 connections
    // (We'll realloc as necessary)
    char localIP[INET6_ADDRSTRLEN] = "localhost";
    FILE *log = fopen("/home/funk/networking/beej/log", "a"); 
    int fd_count = 0;
    int fd_size = 5;
    struct pollfd *pfds = malloc(sizeof *pfds * fd_size);
    // Set up and get a listening socket
    if (argc > 1){
        strcpy(localIP, argv[1]);
    }
    if ((listener = get_listener_socket(localIP)) == -1) {
        fprintf(stderr, "error getting listening socket\n");
        fprintf(log, "error getting listening socket\n");
        fflush(log);
        exit(1);
    }
    // Add the listener to set
    pfds[0].fd = listener;
    pfds[0].events = POLLIN; // Report ready to read on incoming connection
    fd_count = 1; // For the listener

    // Main loop
    for(;;) {
        int poll_count = poll(pfds, fd_count, -1);

        if (poll_count == -1) {
            perror("poll");
            exit(1);
        }
        // Run through the existing connections looking for data to read
        for(int i = 0; i < fd_count; i++) {
            // Check if someone's ready to read
            if (pfds[i].revents & POLLIN) { // We got one!!
                if (pfds[i].fd == listener) {
                    // If listener is ready to read, handle new connection
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } 
                    else {
                        add_to_pfds(&pfds, newfd, &fd_count, &fd_size);
                        printf("pollserver: new connection from %s on "
                            "socket %d\n",
                            inet_ntop(remoteaddr.ss_family,
                                      get_in_addr((struct sockaddr*)&remoteaddr),
                                      remoteIP, INET6_ADDRSTRLEN),
                                      newfd);
                        fprintf(log, "pollserver: new connection from %s on socket %d\n", 
                                                                         remoteIP, newfd);
                        fflush(log);
                        send_online(newfd, fd_count, pfds);

                        for (int k = 1; k < fd_count; ++k) {
                            if (pfds[k].fd != newfd) {
                                sprintf(msg, "%s has joined on sock %d\n", remoteIP, newfd);
                                send(pfds[k].fd, msg, strlen(msg), 0);
                            }
                        }
                    }
                } 
                else {
                    // If not the listener, we're just a regular client
                    int nbytes = recv(pfds[i].fd, buf, sizeof buf, 0);
                    int sender_fd = pfds[i].fd;

                    if (nbytes <= 0) {
                        // Got error or connection closed by client
                        if (nbytes == 0) {
                            // Connection closed
                            printf("pollserver: socket %d hung up\n", sender_fd);
                            fprintf(log, "pollserver: socket %d hung up\n", sender_fd);
                            fflush(log);
                            snprintf(msg, strlen("sock  hung up!\n")+sizeof(int), 
                                    "sock %d hung up!\n", sender_fd);
                            for(int j = 0; j < fd_count; j++) {
                                // Send to everyone!
                                int dest_fd = pfds[j].fd;
                                // Except the listener and ourselves
                                if (dest_fd != listener && dest_fd != sender_fd) {
                                    if (send(dest_fd, msg, strlen(msg), 0) == -1) {
                                        perror("send");
                                    }
                                }
                            }
                        } 
                        else {
                            perror("recv");
                        }
                        close(pfds[i].fd); // Bye!
                        del_from_pfds(pfds, i, &fd_count);
                    } 
                    else {
                        buf[nbytes] = '\0';
                        snprintf(msg, MSG_LEN, "sock %d: %s", sender_fd, buf);
                        fprintf(log, "%s", msg);
                        fflush(log);
                        buf[strcspn(buf, "\n")] = 0;

                        if (handle_command(buf, sender_fd)) {
                            continue;
                        }
                        // We got some good data from a client
                        for(int j = 0; j < fd_count; j++) {
                            // Send to everyone!
                            int dest_fd = pfds[j].fd;

                            // Except the listener and ourselves
                            if (dest_fd != listener && dest_fd != sender_fd) {
                                if (send(dest_fd, msg, strlen(msg), 0) == -1) {
                                    perror("send");
                                }
                            }
                        }
                    }
                } // END handle data from client
            } // END got ready-to-read from poll()
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    return 0;
}


int fd_from_adr(char *adr, struct pollfd *pfds, int fd_count,
                char addresses[FD_RANGE][INET6_ADDRSTRLEN]) {
    int fd;

    for (int i = 0; i < fd_count; ++i) {
        fd = pfds[i].fd;
        printf("%s %s\n", addresses[fd], adr);
        printf("%lu %lu\n", strlen(addresses[fd]), strlen(adr));
        if (strcmp(adr, addresses[fd]) == 0) {
            return fd;
        }
    }
    return -1;
}


void send_online(int new_sock, int fd_count, struct pollfd *pfds) {
    char user[strlen("sock \n") + sizeof(int)];
    send(new_sock, "Online:\n", strlen("Online:\n"), 0);
    for (int i = 1; i < fd_count; ++i) {
        if (pfds[i].fd != new_sock) {
            sprintf(user, "sock %d\n", pfds[i].fd);
            send(new_sock, user, strlen(user), 0);
        }
        else {
            send(new_sock, "You\n", strlen("You\n"), 0);
        }
    }
}


// Get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


// Return a listening socket
int get_listener_socket(char *ip_address) {
    int listener;     // Listening socket descriptor
    int yes=1;        // For setsockopt() SO_REUSEADDR, below
    int rv;

    struct addrinfo hints, *ai, *p;

    // Get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(ip_address, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) { 
            continue;
        }
        // Lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }
        break;
    }
    freeaddrinfo(ai); // All done with this
    // If we got here, it means we didn't get bound
    if (p == NULL) {
        return -1;
    }
    // Listen
    if (listen(listener, 10) == -1) {
        return -1;
    }
    return listener;
}


// Add a new file descriptor to the set
void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size) {
    // If we don't have room, add more space in the pfds array
    if (*fd_count == *fd_size) {
        *fd_size *= 2; // Double it
        *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
    }
    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN; // Check ready-to-read
    (*fd_count)++;
}


// Remove an index from the set
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count) {
    // Copy the one from the end over this one
    pfds[i] = pfds[*fd_count-1];
    (*fd_count)--;
}
