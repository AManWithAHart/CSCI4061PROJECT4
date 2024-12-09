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

// Global master socket fd for server
static int master_fd = -1;

// Lock to ensure thread safe operations
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

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
    // Set up server address structure
    struct sockaddr_in master_addr;
    master_addr.sin_family = AF_INET; // same a PF_INET
    master_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    master_addr.sin_port = htons(port);

    // Create socket
    master_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (master_fd < 0) {
        perror("Failed to create socket file descriptor");
        exit(EXIT_FAILURE);
    }

    // Enable socket reuse to avoid "already in use" errors
    int enable = 1;
    if (setsockopt(master_fd, SOL_SOCKET, SO_REUSEADDR, (char *) &enable, sizeof(enable)) < 0) {
        perror("Failed to set sock options for socket");
        exit(EXIT_FAILURE);
    }

    // Bind socket to server address
    if (bind(master_fd, (struct sockaddr *) &master_addr, sizeof(master_addr)) < 0) {
        perror("Failed to bind socket to server address");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(master_fd, MAX_QUEUE_LEN) < 0) {
        perror("call to listen() failed in init()");
        exit(EXIT_FAILURE);
    }

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
    socklen_t address_size = sizeof(struct sockaddr_in);

    // Grab lock
    if (pthread_mutex_lock(&mtx) != 0) {
        perror("Failed to acquire lock");
        return -1;
    }

    // Accept incoming connection
    int client_fd = accept(master_fd, &client_sock, &address_size);
    if (client_fd < 0) {
        printf("FAILED ACCEPTED\n");
        return -1;
    }

    // Release lock
    if (pthread_mutex_unlock(&mtx) != 0) {
        perror("Failed to release lock");        
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
    // Create packet with file size
    packet_t packet = {.size = (unsigned) size};

    // Send packet header with file size
    if (send(socket, &packet, sizeof(packet_t), 0) < sizeof(packet_t)) {
        perror("Failed to send packet header");
        return -1;
    }

    // Send the actual data
    if (send(socket, buffer, (unsigned) size, 0) < size) {
        perror("Failed to send file data");
        return -1;
    }

    return 0;
}


/**********************************************
 * get_request_server
   - fd is the file descriptor for the socket
   - filelength is a pointer to a size_t variable that will be set to the length of the file
   - returns a pointer to the file data
************************************************/
char * get_request_server(int fd, size_t *filelength) {
    // Receive the packet with file size
    packet_t packet;
    ssize_t bytes_read = recv(fd, &packet, sizeof(packet_t), 0);
    if (bytes_read < sizeof(packet_t)) {
    	perror("Packet size was not properly read");
	    return NULL;
    }

    // Get file size from packet
    size_t size = packet.size;
    
    // Allocate buffer for file data
    char *buffer = (char *) malloc(size);
    
    // Receive file data
    bytes_read = recv(fd, buffer, size, 0);
    if (bytes_read != size) {
    	perror("file was not properly read");
        free(buffer);
	    return NULL;
    }

    *filelength = size;
    return buffer; // Caller must free buffer
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
int setup_connection(int port) {
    // Create socket for server connection
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
	    perror("Failed to create a socket for server connection");
	    exit(EXIT_FAILURE);
    }

    // Configure server adderess
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    serveraddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Attempt connection
    if (connect(sockfd, (struct sockaddr *) &serveraddr, sizeof(struct sockaddr_in)) < 0) {
	    perror("client failed to conect to server");
        close(sockfd);
        exit(EXIT_FAILURE);
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
int send_file_to_server(int socket, FILE *file, int size) {
    // Ensure file pointer is at the beginning
    rewind(file);

    // Send file size packet
    packet_t packet;
    packet.size = size;
    if (send(socket, &packet, sizeof(packet_t), 0) < sizeof(packet_t)) {
        perror("Error sending packet to server");
        return -1;
    }

    // Allocate a buffer to read file contents
    char *buffer = malloc(size);
    if (buffer == NULL) {
        perror("Memory allocation failed");
        return -1;
    }

    // Read file contents into buffer
    if (fread(buffer, 1, size, file) != size) {
        perror("Error reading file");
        free(buffer);
        return -1;
    }

    // Send the file data
    if (send(socket, buffer, size, 0) != size) {
        perror("Error sending file data to server");
        free(buffer);
        return -1;
    }

    free(buffer);
    return 0;
}

/**********************************************
 * receive_file_from_server
   - socket is the file descriptor for the socket
   - filename is the name of the file you want to save
   - returns 0 on success, -1 on failure
************************************************/
int receive_file_from_server(int socket, const char *filename) {
    // Receive the packet with file size
    packet_t packet;
    if (recv(socket, &packet, sizeof(packet_t), 0) < sizeof(packet_t)) {
        perror("Error receiving packet from server");
        return -1;
    }

    // Get file size
    int size = packet.size;

    // Open the file for writing in binary mode
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("Failed to open file for writing");
        return -1;
    }

    // Allocate buffer to receive file contents
    char *buffer = malloc(size);
    if (buffer == NULL) {
        perror("Memory allocation failed for file buffer");
        fclose(file);
        return -1;
    }

    // Receive file data
    ssize_t bytes_read = recv(socket, buffer, size, 0);
    if (bytes_read < size) {
        perror("Error receiving file data from server");
        free(buffer);
        fclose(file);
        return -1;
    }

    // Write buffer contents to file
    size_t bytes_written = fwrite(buffer, 1, size, file);
    if (bytes_written != size) {
        perror("Failed to write file contents");
        free(buffer);
        fclose(file);
        return -1;
    }

    // Clean up
    free(buffer);
    fclose(file);
    return 0;
}
