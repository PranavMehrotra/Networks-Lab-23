#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFSIZE 1024
#define MAX_SIZE 500

char *recieve_expr(int newsockfd, int *recv_size){
	char c;
	char *s;
	// printf("\n");
	int len=0,size = MAX_SIZE,y,chunk=400;
	s = (char *)malloc(sizeof(char)*size);
	if(!s)	return NULL;
	// printf("Hello\n");
    int i=1;
	while((y=recv(newsockfd,s+len,chunk,0))>0){
		printf("%d ",i++);
        len+=y;
		// if(s[len-1]=='\0')	break;
		while(len+chunk>=size){
			s = realloc(s,sizeof(char)*(size*=2));
			if(!s)	return NULL;
			// printf("\t%d",size);
		}
	}
	if(len==0){
		free(s);
		return NULL;
	}
	s = realloc(s,sizeof(char)*(len+1));
    *recv_size = len;
	printf("\n");
    return s;
}

// void parse_http_response(char* response, size_t response_len, char** headers, char** content) {
//     const char* header_end = strstr(response, "\r\n\r\n");
//     if (header_end == NULL) {
//         // Handle error: no header end found
//         return;
//     }
//     size_t header_len = header_end - response + 4;
//     *headers = malloc(header_len + 1);
//     strncpy(*headers, response, header_len);
//     (*headers)[header_len] = '\0';

//     size_t content_len = response_len - header_len;
//     *content = malloc(content_len);
//     memcpy(*content, response + header_len, content_len);
//     free(response);
// }

void parse_http_response(char* response, int response_len, char** headers, char** content, int* content_length, char **content_type) {
    const char* header_end = strstr(response, "\r\n\r\n");
    if (header_end == NULL) {
        // Handle error: no header end found
        return;
    }
    size_t header_len = header_end - response + 4;
    *headers = malloc(header_len + 1);
    strncpy(*headers, response, header_len);
    (*headers)[header_len] = '\0';

    size_t content_len = response_len - header_len;
    *content = malloc(content_len);
    memcpy(*content, response + header_len, content_len);
    free(response);

    // Parse the headers to get the content length
    char parse_header[100];
    strcpy(parse_header,"Content-Length: ");
    char* parse_header_start = strstr(*headers, parse_header);
    if (parse_header_start == NULL) {
        // Handle error: content length header not found
        printf("Content length header not found\n");
        *content_length = content_len;
        return;
    }
    parse_header_start += strlen(parse_header);
    *content_length = atoi(parse_header_start);
    // Parse the headers to get the content type
    strcpy(parse_header,"Content-Type: ");
    parse_header_start = strstr(*headers, parse_header);
    if (parse_header_start == NULL) {
        // Handle error: content length header not found
        printf("Content type header not found\n");
        *content_type = strdup("txt");
        return;
    }
    parse_header_start += strlen(parse_header);
    // Parse the parse_header_start to get the content type, with delimeter '/'
    parse_header_start = strstr(parse_header_start, "/");
    parse_header_start++;
    // Take until '\r\n'
    char* parse_header_end = strstr(parse_header_start, "\r\n");
    // Copy the content type
    *content_type = malloc(parse_header_end - parse_header_start + 1);
    strncpy(*content_type, parse_header_start, parse_header_end - parse_header_start);
    (*content_type)[parse_header_end - parse_header_start] = '\0';
}

int main(int argc, char *argv[]) {
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[BUFSIZE];

    if (argc != 2) {
        fprintf(stderr,"usage %s url\n", argv[0]);
        exit(0);
    }

    // portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    // struct timeval timeout;
    // timeout.tv_sec = 30;
    // timeout.tv_usec = 0;
    // if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    // {
    //     perror("setsockopt failed\n");
    //     exit(EXIT_FAILURE);
    // }
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    portno = 80;
    char hostname[256];

    // Parse the URL string to extract the hostname (domain name)
    if (sscanf(argv[1], "http://%255[^:]:%d", hostname, &portno) == 2) {
        // Port is specified in the URL
    } else if (sscanf(argv[1], "http://%255[^/]", hostname) == 1) {
        // Port is not specified, use the default
        // printf("powe\n");
    } else {
        // URL format is invalid
        printf("Please enter a valid URL\n");
        return 0;
    }
    printf("%s\n", hostname);
    char *parse_filename = strrchr(argv[1], '/');
    char *filename;
    if (parse_filename == NULL) {
        filename = strdup("test");
    }
    else{
        parse_filename++;
        char *filetype = strstr(parse_filename, ".");
        filename = malloc(filetype - parse_filename + 1);
        strncpy(filename, parse_filename, filetype - parse_filename);
        filename[filetype - parse_filename] = '\0';
    }
    // strcpy(hostname,argv[1]);
    // Perform a DNS lookup to get the IP address
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    // printf("%s\n",server->h_addr);
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    // printf("heelo\n");
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
        perror("ERROR connecting");
        exit(1);
    }
    // printf("hello22\n");

    /* Construct HTTP GET request */
    sprintf(buffer, "GET %s HTTP/1.1\r\n", argv[1]);
    strcat(buffer, "Host: ");
    strcat(buffer, argv[1]);
    strcat(buffer, "\r\n");
    strcat(buffer, "Accept: */*\r\n");
    strcat(buffer, "Connection: close\r\n");
    strcat(buffer, "\r\n");
    // time_t t1 = time(NULL);
    int total = 0;
    int bytesReceived = 0;
    // clock_t t1 = clock();
    n = send(sockfd, buffer, strlen(buffer),0);
    if (n < 0) {
        perror("ERROR writing to socket");
        exit(1);
    }
    // printf("Hello\n");
    // bzero(buffer, BUFSIZE);
    int resp_size=0;
    char *s = recieve_expr(sockfd,&resp_size);
    /* Save content to file */
    if(s==NULL || resp_size==0){
        printf("No Response\n");
        return 0;
    }
    char *headers, *content;
    int content_length;
    char *content_type;
    parse_http_response(s, resp_size, &headers, &content,&content_length, &content_type);
    printf("\n\n");
    printf("%s\n", headers);
    printf("%d\n", content_length);
    FILE *fp;
    char *file_name = malloc(strlen(content_type) + strlen(filename) + 2);
    strcpy(file_name, filename);
    strcat(file_name, ".");
    strcat(file_name, content_type);
    printf("%s\n", file_name);
    fp = fopen(file_name, "w");
    fwrite(content, sizeof(char), content_length, fp);
    fclose(fp);
    free(headers);
    free(content);
    close(sockfd);
    int pid = fork();
    if(pid==0){
        char *args[] = {"xdg-open", file_name, NULL};
        execvp(args[0], args);
        perror("xdg-open");
        exit(0);
    }
        /* Open file in Google Chrome */
    // char *args[] = {"google-chrome", "response.html", NULL};
    // execvp(args[0], args);

    return 0;
}
//./b 203.110.245.250 80 http://cse.iitkgp.ac.in/~agupta/networks/index.html
//./b 203.110.245.250 80 http://cse.iitkgp.ac.in/~agupta/networks/1-Introduction.pdf
// ./b 188.184.21.108 80 http://info.cern.ch/hypertext/WWW/TheProject.html
