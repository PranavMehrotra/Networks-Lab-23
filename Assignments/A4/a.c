#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFSIZE 1024

int main(int argc, char *argv[]) {
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[BUFSIZE];

    if (argc < 3) {
        fprintf(stderr,"usage %s hostname port\n", argv[0]);
        exit(0);
    }

    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
        perror("ERROR connecting");
        exit(1);
    }

    /* Construct HTTP GET request */
    sprintf(buffer, "GET %s HTTP/1.1\r\n", argv[3]);
    strcat(buffer, "Host: ");
    strcat(buffer, argv[1]);
    strcat(buffer, "\r\n");
    strcat(buffer, "Connection: close\r\n");
    strcat(buffer, "\r\n");

    n = write(sockfd, buffer, strlen(buffer));
    if (n < 0) {
        perror("ERROR writing to socket");
        exit(1);
    }

    bzero(buffer, BUFSIZE);

    int total = 0;
    int bytesReceived = 0;
    while ((bytesReceived = read(sockfd, buffer + total, BUFSIZE - total - 1)) > 0) {
        total += bytesReceived;
    }
    printf("%d\n", total);

    if (bytesReceived < 0) {
        perror("ERROR reading from socket");
        exit(1);
    }
    printf("%s ", buffer);
    /* Save content to file */
    FILE *fp;
    fp = fopen("response.html", "w");
    fwrite(buffer, sizeof(char), total, fp);
    fclose(fp);

        /* Open file in Google Chrome */
    char *args[] = {"google-chrome", "response.html", NULL};
    execvp(args[0], args);

    return 0;
}
//./a.out 203.110.245.250 80 http://cse.iitkgp.ac.in/~agupta/networks/index.html