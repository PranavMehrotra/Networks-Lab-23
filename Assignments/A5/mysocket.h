#ifndef MYSOCKET_H
#define MYSOCKET_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include <poll.h>

#define SOCK_MyTCP 128
#define MAX_MSG_SIZE 5000
#define MAX_CHUNK_SIZE 1000
#define SLEEP_TIME 1
#define MY_CLOSE_SLEEP 5
#define POLL_TIMEOUT 1000

// Structure for storing a linked list with front and rear pointers for the send and receive queues of a socket
typedef struct _queue
{
    struct _queue *next;
    char *data;
    int len;
}queue;

// Structure for storing the front and rear pointers of the send and receive queues of a socket
typedef struct _queue_head
{
    queue *front;
    queue *rear;
    /* 
    If front == rear, then the queue is empty
    One dummy node is always present in the queue at the beginning, so front and rear will never be NULL
    If you want to add a node to the queue, add it after the rear node and then move the rear pointer to the new node
    If you want to remove a node from the queue, remove the node at the front->next and then update the front->next to the new node
    Also if front->next == rear, then the queue has only one node, so after removing the node, set front->next = NULL and rear = front
    */
}queue_head;



int my_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int my_accept(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen);
int my_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int my_socket(int domain, int type, int protocol);
int my_listen(int sockfd, int backlog);
int my_close(int fd);
ssize_t my_send(int sockfd, const void *buf, size_t len, int flags);
ssize_t my_recv(int sockfd, void *buf, size_t len, int flags);

#endif