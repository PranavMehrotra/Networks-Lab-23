// A Simple Client Implementation
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <poll.h>
  
#define MAX_SIZE 100

int main() { 
    int sockfd; 
    struct sockaddr_in serv_addr; 
  
    // Creating socket file descriptor 
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if ( sockfd < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
  
    memset(&serv_addr, 0, sizeof(serv_addr)); 
      
    // Server information 
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(8181); 
    inet_aton("127.0.0.1", &serv_addr.sin_addr); 
      
    int t=0,ret,n;
    char *hello = "CLIENT:HELLO"; 
    socklen_t len;
    struct pollfd fdset[1];
    fdset[0].fd = sockfd;
    fdset[0].events = POLLIN;
    int timeout = 1000; 
    char buf[MAX_SIZE];
    len = sizeof(serv_addr);
    while(t<5){
        sendto(sockfd, (const char *)hello, strlen(hello), 0, 
			(struct sockaddr *) &serv_addr, len); 
        t++;
        printf("Try number: %d\n",t);
        ret = poll(fdset,1,timeout);
        if(ret>0){
            n=recvfrom(sockfd, buf, MAX_SIZE,0,(struct sockaddr *) &serv_addr, &len);
            buf[n]= '\0';
            printf("Current Server Date and Time: %s\n",buf);
            exit(0);
        }
    }

    printf("Timeout exceeded! \n"); 
           
    close(sockfd); 
    return 0; 
} 