#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define FD_RANGE      20
#define FILE_BUF      1024
#define MSG_LEN       275
#define FILEPATH_LEN  50
#define END           "::CSJDNNYTE7::::::::EOF::" 

static bool pause_recv = false;
static bool pause_send = false;

#ifndef __FILETRANS__
#define __FILETRANS__


bool is_file_command(char *msg);
int file_command(char *msg, int sock); 
int send_file(int target, int fd);
int file_transfer(char *msg, int src);
int recv_file(int sock, char *filepath);
int quick_file_transfer(int src, int dst, char *msg);
int send_though_server(char *msg, int sock);
bool handle_command(char msg[MSG_LEN], int src_client);
bool client_command(char *msg, int dst_client);
bool incoming_command(char *msg, int src_sock);
bool is_recv_command(char *msg);
#endif
