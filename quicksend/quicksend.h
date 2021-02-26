

#ifndef __QUICKSEND__
#define __QUICKSEND__   
#define MAX_BYTES  200
#define FILE_BUF   1024
#define QUE        10

int send_file_from_sock(char *filepath, char *ip, char *port);
int recv_file_on_sock(char *filepath, char *ip, char *port);

#endif
