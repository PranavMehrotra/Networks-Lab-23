// A Simple UDP Server that sends a HELLO message
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <time.h>
#include <poll.h>

#define MAX_SIZE 100
  
int main() { 
    int sockfd; 
    struct sockaddr_in servaddr, cliaddr; 
      
    // Create socket file descriptor 
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if ( sockfd < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
      
    memset(&servaddr, 0, sizeof(servaddr)); 
    memset(&cliaddr, 0, sizeof(cliaddr)); 
      
    servaddr.sin_family    = AF_INET; 
    servaddr.sin_addr.s_addr = INADDR_ANY; 
    servaddr.sin_port = htons(8181); 
      
    // Bind the socket with the server address 
    if ( bind(sockfd, (const struct sockaddr *)&servaddr,  
            sizeof(servaddr)) < 0 ) 
    { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 
    
    printf("\nServer Running....\n");
  
    int n=0,f=1; 
    socklen_t len; 
    time_t rawtime;
	struct tm * timeinfo;
    struct pollfd fdset[1];
    fdset[0].fd = sockfd;
    fdset[0].events = POLLIN;
    int timeout = 1000; 
    char buf[MAX_SIZE];
    len = sizeof(cliaddr);
    while(1){
        printf("Waiting!\n");
        // while(n<=0){
        //     n=poll(fdset,1,timeout);
        // }
        n=recvfrom(sockfd, (char *)buf, MAX_SIZE, 0, 
        ( struct sockaddr *) &cliaddr, &len);
        if(n>0){
        // if(n>0){
            // printf("\n%s\n",buf);
            // n=0;
            // printf("Sleep Start!\n");
            // sleep(4);
            // printf("Sleep End!\n");
            time ( &rawtime );
            timeinfo = localtime ( &rawtime );

            strcpy(buf, asctime(timeinfo));
            sendto(sockfd, buf, strlen(buf) + 1, 0,( struct sockaddr *) &cliaddr, len);
            printf("Time Sent!\n");
        }
        // memset(buf,0,MAX_SIZE);
        // }
        // n=0;
    }
    // buf[n] = '\0'; 
    // printf("%s\n", buf);

    close(sockfd);
    return 0; 
} 