#include "../include/utils.h"
#include "../include/server.h"
#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // bzero()
#include <sys/socket.h>
#include <unistd.h> // read(), write(), close()
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <stdint.h>

int master_fd; // should this be master_fd?
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
struct sockaddr_in master_addr;

/*
################################################
##############Server Functions##################
################################################
*/

/**********************************************
 * init
   - port is the number of the port you want the server to be
     started on
   - initializes the connection acception/handling system
   - if init encounters any errors, it will call exit().
************************************************/
void init(int port) {

    int sockfd;
    master_addr.sin_family = AF_INET; // same a PF_INET
    master_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    master_addr.sin_port = htons(port);
    /**********************************************
    * IMPORTANT!
    * ALL TODOS FOR THIS FUNCTION MUST BE COMPLETED FOR THE INTERIM SUBMISSION!!!!
    **********************************************/

    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Failed to create socket file descriptor");
        exit(EXIT_FAILURE);
    }

    // may be a better way than decl variable, going off slides for now
    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *) &enable, sizeof(int)) == -1) {
        perror("Failed to set sock options for socket");
        exit(EXIT_FAILURE);
    }

    if (bind(sockfd, (struct sockaddr *) &master_addr, sizeof(master_addr)) == -1) {
        perror("Failed to bind socket to server address");
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, MAX_QUEUE_LEN)) {
        perror("call to listen() failed in init()");
        exit(EXIT_FAILURE);
    }

    // We save the file descriptor to a global variable so that we can use it in accept_connection().
    master_fd = sockfd;

    printf("UTILS.O: Server Started on Port %d\n",port);
    fflush(stdout);
}


/**********************************************
 * accept_connection - takes no parameters
   - returns a file descriptor for further request processing.
   - if the return value is negative, the thread calling
     accept_connection must should ignore request.
***********************************************/
int accept_connection(void) {

    struct sockaddr_in client_sock;
    unsigned client_sock_len = sizeof(client_sock);

   /**********************************************
    * IMPORTANT!
    * ALL TODOS FOR THIS FUNCTION MUST BE COMPLETED FOR THE INTERIM SUBMISSION!!!!
    **********************************************/

    if (pthread_mutex_lock(&mtx) != 0) {
        return -1;
    }

    int client_fd;
    if ((client_fd = accept(master_fd, (struct sockaddr *) &master_addr, &client_sock_len)) == -1) {
        return -1; // accept failed, return negative value (ignore request)
    }

    if (pthread_mutex_unlock(&mtx) != 0) {
        return -1;
    }

    return client_fd;
}


/**********************************************
 * send_file_to_client
   - socket is the file descriptor for the socket
   - buffer is the file data you want to send
   - size is the size of the file you want to send
   - returns 0 on success, -1 on failure
************************************************/
int send_file_to_client(int socket, char * buffer, int size) {
    packet_t packet = {.size = (unsigned) size}; // moderately faster way to init a struct

    if (write(socket, &packet, sizeof(packet_t)) < sizeof(packet_t)) {
        return -1;
    }

    if (write(socket, buffer, (unsigned) size) < size) {
        return -1;
    }
    // no failure, success!
    return 0;
}


/**********************************************
 * get_request_server
   - fd is the file descriptor for the socket
   - filelength is a pointer to a size_t variable that will be set to the length of the file
   - returns a pointer to the file data
************************************************/
char * get_request_server(int fd, size_t *filelength)
{
    //TODO: create a packet_t to hold the packet data
    packet_t packet;
    //TODO: receive the response packet
    if (read(fd, &packet, sizeof(packet_t)) < sizeof(packet_t)) {
    	perror("packet was not properly read");
	    exit(EXIT_FAILURE); // should maybe just be return NULL, unclear
    }
    //TODO: get the size of the image from the packet
    unsigned size = packet.size;
    //TODO: recieve the file data and save into a buffer variable.
    char *buf = (char *) malloc(size);
    if (read(fd, buf, size) < size) {
    	perror("file was not properly read");
	    exit(EXIT_FAILURE); // should maybe just be return NULL, unclear
    }
    //TODO: return the buffer

    return buf; // caller should free buf
}


/*
################################################
##############Client Functions##################
################################################
*/

/**********************************************
 * setup_connection
   - port is the number of the port you want the client to connect to
   - initializes the connection to the server
   - if setup_connection encounters any errors, it will call exit().
************************************************/
int setup_connection(int port)
{
    //TODO: create a sockaddr_in struct to hold the address of the server
    struct sockaddr_in serveraddr;

    //TODO: create a socket and save the file descriptor to sockfd
    int sockfd;
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
	perror("failed to create a socket for server in setup_connection");
	exit(EXIT_FAILURE);
    }

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (connect(sockfd, (struct sockaddr *) &serveraddr, sizeof(struct sockaddr_in)) != 0) {
	perror("client failed to conect to server");
    }
    return sockfd;
}


/**********************************************
 * send_file_to_server
   - socket is the file descriptor for the socket
   - file is the file pointer to the file you want to send
   - size is the size of the file you want to send
   - returns 0 on success, -1 on failure
************************************************/
int send_file_to_server(int socket, FILE *file, int size)
{
    //TODO: send the file size packet
    packet_t packet;
    packet.size = size;
    if (write(socket, &packet, sizeof(packet_t)) < sizeof(packet_t)) {
        perror("Error sending packet to server");
        return -1;
    }
    //TODO: send the file data
    if (write(socket, file, size) < size) {
        perror("Error sending file data to server");
        return -1;
    }

    // TODO: return 0 on success, -1 on failure
    return 0; // temporarily added to satisfy compiler warnings
}

/**********************************************
 * receive_file_from_server
   - socket is the file descriptor for the socket
   - filename is the name of the file you want to save
   - returns 0 on success, -1 on failure
************************************************/
int receive_file_from_server(int socket, const char *filename)
{
    //TODO: create a buffer to hold the file data
    // Not sure if this is needed to be malloc'd
    char *buf;

    //TODO: open the file for writing binary data
    FILE *f = fopen(filename, "wb");
    if (f == NULL) {
        perror("Failed to open file from receive_file_from_server\n");
        return -1;
    }
   //TODO: create a packet_t to hold the packet data
    packet_t packet;
   //TODO: receive the response packet
    if (read(socket, &packet.size, sizeof(packet_t)) < 0) {
        perror("Error receiving packet from server\n");
        return -1;
    }

    //TODO: get the size of the image from the packet
    int size = packet.size;
    buf = malloc(sizeof(char) * size);
    if (buf == NULL) {
        perror("Buffer is null when receiving file\n");
        return -1;
    }

   //TODO: recieve the file data and write it to the file
    if (read(socket, buf, size) < 0) {
        perror("Error receiving file data from server\n");
        return -1;
    }
    if (fwrite(buf, size, 1, f) < 0) {
        perror("Failed to write to file\n");
        return -1;
    }
    free(buf);
    //TODO: return 0 on success, -1 on failure
    return 0; // temporarily added to satisfy compiler warnings

}

