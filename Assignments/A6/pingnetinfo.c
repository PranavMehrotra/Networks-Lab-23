#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <sys/stat.h>
#include <time.h>
#include <poll.h>
#include <arpa/inet.h>


#define BUFSIZE 1024
#define MAX_SIZE 500
#define MAX_HEADER_SIZE 256


int main(int argc, char **argv) {

    if(argc != 4){
        printf("Enter more arguments\n");
        return 0;
    }


    int sockfd, portno, n,t;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    portno = 80;  //default port number
    char hostname[BUFSIZE];
    char command[20],*url,buffer[3*BUFSIZE],filename[256];
    char *parse_filename;

        url = argv[1];
        n = atoi(argv[2]);
        t = atoi(argv[3]);
        
        // Parse the URL string to extract the hostname (domain name)
        if (sscanf(url, "http://%1000[^:]:%d", hostname, &portno) == 2) {
            // Port is specified in the URL
            char temp_url[BUFSIZE];
            sscanf(url, "http://%255[^/:]%1000[^:]", hostname,temp_url);
            strcpy(url,temp_url);
            // printf("URL: %s\n Port: %d \n Host: %s\n",url,portno,hostname);
        } else if (sscanf(url, "http://%1000[^/]", hostname) == 1) {
            // Port is not specified, use the default
            char temp_url[BUFSIZE];
            sscanf(url, "http://%255[^/:]%1000[^:]", hostname,temp_url);
            strcpy(url,temp_url);
            // printf("URL: %s\n Host: %s\n",url,hostname);
        }
         if (sscanf(url, "%1000[^:]:%d", hostname, &portno) == 2) {
            // Port is specified in the URL
            char temp_url[BUFSIZE];
            sscanf(url, "%255[^/:]%1000[^:]", hostname,temp_url);
            strcpy(url,temp_url);
            // printf("URL: %s\n Port: %d \n Host: %s\n",url,portno,hostname);
        } else if (sscanf(url, "%1000[^/]", hostname) == 1) {
            // Port is not specified, use the default
            char temp_url[BUFSIZE];
            sscanf(url, "%255[^/:]%1000[^:]", hostname,temp_url);
            strcpy(url,temp_url);
            // printf("URL: %s\n Host: %s\n",url,hostname);
        } else {
            // URL format is invalid
            printf("Please enter a valid HTTP URL\n");
        }


        // printf("%s\n", hostname);
        // Perform a DNS lookup to get the IP address
        server = gethostbyname(hostname);
        if (server == NULL) {
            fprintf(stderr,"ERROR, no such host\n");
        }
    printf("Site to probe: %s \n", inet_ntoa(*(struct in_addr*)(server->h_addr_list[0])));
    printf("Number of times a probe is sent per link: %d \n", n);
    printf("Time difference between probes: %d \n", t);
       
    return 0;
}
