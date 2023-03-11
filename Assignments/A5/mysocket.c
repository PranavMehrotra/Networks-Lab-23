#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/stat.h>
#include<errno.h>
#include "mysocket.h"


int my_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    return bind(sockfd, addr, addrlen);
}

int my_listen(int sockfd, int backlog)
{
    return listen(sockfd, backlog);
}

int my_close(int fd)
{
    //clean send receieve table
    //delete send receiev table
    return close(fd);
}

int my_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    return connect(sockfd,addr,addrlen);
}

int my_accept(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen)
{
    return accept(sockfd, addr, addrlen);
}

ssize_t my_send(int sockfd, const void *buf, size_t len, int flags)
{

}
ssize_t my_recv(int sockfd, void *buf, size_t len, int flags)
{
    
}

//r wait on recv
//receive data via multiple call
//interpret and form message
//put in receieve table

//s sleep for t time
//wake
//see if message pending
//send in one or more call


int my_socket(int domain, int type, int protocol)
{
    int sockfd;
    if(domain == SOCK_MyTCP)sockfd  = socket(AF_INET, type, protocol);
    else sockfd = socket(domain, type, protocol);

    //create R and S
    //Initialise send recieve table

    return sockfd;
}
int main(void)
{
    printf("%d", SOCK_MyTCP);
}