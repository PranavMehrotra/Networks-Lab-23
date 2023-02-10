#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>

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
    // int i=1;
	while((y=recv(newsockfd,s+len,chunk,0))>0){
		// printf("%d ",i++);
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
	// printf("\n");
    return s;
}

void send_expr(int newsockfd, char *expr, int size){
	int len=0,y,chunk=MAX_SIZE;
	if(size<chunk){
		send(newsockfd,expr,size+1,0);
		return;
	}
	while(1){
		if(len+chunk>size){
			send(newsockfd,expr+len,size-len+1,0);
			break;
		}
		y=send(newsockfd,expr+len,chunk,0);
		len+=y;
	}
}

int parse_http_response(char* response, int response_len, char** headers, char** content, int* content_length, char **content_type) {
    const char* header_end = strstr(response, "\r\n\r\n");
    if (header_end == NULL) {
        // Handle error: no header end found
        return 1;
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
        printf("Content length set to %ld\n", content_len);
        *content_length = content_len;
    }
    else{
        parse_header_start += strlen(parse_header);
        *content_length = atoi(parse_header_start);
    }
    // Parse the headers to get the content type
    strcpy(parse_header,"Content-Type: ");
    parse_header_start = strstr(*headers, parse_header);
    if (parse_header_start == NULL) {
        // Handle error: content length header not found
        printf("Content type header not found\n");
        *content_type = strdup("txt");
    }
    else{
        parse_header_start += strlen(parse_header);
        // Parse the parse_header_start to get the content type, with delimeter '/'
        parse_header_start = strstr(parse_header_start, "/");
        parse_header_start++;
        // Take until ';'
        char* parse_header_end = strstr(parse_header_start, ";");
        if(parse_header_end == NULL){
            // Take until '\r\n'
            parse_header_end = strstr(parse_header_start, "\r\n");
        }
        *content_type = malloc(parse_header_end - parse_header_start + 1);
        strncpy(*content_type, parse_header_start, parse_header_end - parse_header_start);
        (*content_type)[parse_header_end - parse_header_start] = '\0';
    }
    return 0;
}

int main() {
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    portno = 80;
    char hostname[BUFSIZE];
    char command[20],url[BUFSIZE],buffer[2*BUFSIZE],filename[256];
    char *parse_filename;
    while(1){
        printf("MyOwnBrowser> ");
        scanf("%s",command);
        if(strcasecmp(command,"QUIT")==0)   break;
        scanf("%s",url);
        if(strcasecmp(command,"PUT")==0)    scanf("%s",filename);
        // Parse the URL string to extract the hostname (domain name)
        if (sscanf(url, "http://%255[^:]:%d", hostname, &portno) == 2) {
            // Port is specified in the URL
        } else if (sscanf(url, "http://%255[^/]", hostname) == 1) {
            // Port is not specified, use the default
        } else {
            // URL format is invalid
            printf("Please enter a valid HTTP URL\n");
            continue;
        }
        printf("%s\n", hostname);
        // Perform a DNS lookup to get the IP address
        server = gethostbyname(hostname);
        if (server == NULL) {
            fprintf(stderr,"ERROR, no such host\n");
            continue;
        }
        // printf("%s\n",server->h_addr);
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("ERROR opening socket");
            exit(1);
        }
        memset((char *) &serv_addr, 0,sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
        serv_addr.sin_port = htons(portno);
        if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
            perror("ERROR connecting");
            continue;
        }

        if(strcasecmp(command,"GET")==0){

            parse_filename = strrchr(url, '/');
            if (parse_filename == NULL) {
                strcpy(filename,"test");
            }
            else{
                parse_filename++;
                char *filetype = strstr(parse_filename, ".");
                if (filetype == NULL) {
                    strcpy(filename,"test");
                }
                else{
                    // filename = malloc(filetype - parse_filename + 1);
                    strncpy(filename, parse_filename, filetype - parse_filename);
                    filename[filetype - parse_filename] = '\0';
                }
            }
            /* Construct HTTP GET request */
            sprintf(buffer, "GET %s HTTP/1.1\r\n", url);
            strcat(buffer, "Host: ");
            strcat(buffer, hostname);
            strcat(buffer, "\r\n");
            strcat(buffer, "Accept: */*\r\n");
            strcat(buffer, "Connection: close\r\n");
            strcat(buffer, "\r\n");
            // time_t t1 = time(NULL);
            int total = 0;
            int bytesReceived = 0;
            // clock_t t1 = clock();
            send_expr(sockfd, buffer, strlen(buffer));
            // printf("Hello\n");
            // bzero(buffer, BUFSIZE);
            int resp_size=0;
            char *s = recieve_expr(sockfd,&resp_size);
            /* Save content to file */
            if(s==NULL || resp_size==0){
                printf("No Response\n");
                continue;
            }
            char *headers, *content;
            int content_length;
            char *content_type;
            if(parse_http_response(s, resp_size, &headers, &content,&content_length, &content_type)){
                printf("Error in parsing the HTTP response\n");
                continue;
            }
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
            free(content_type);
            close(sockfd);
            int pid = fork();
            if(pid==0){
                char *args[] = {"xdg-open", file_name, NULL};
                freopen("/dev/null", "w", stdout);
                freopen("/dev/null", "w", stderr);
                execvp(args[0], args);
                perror("xdg-open");
                exit(0);
            }
            free(file_name);
        }
        else if(strcasecmp(command,"PUT")==0){
            /* Construct HTTP PUT request */
            sprintf(buffer, "PUT %s HTTP/1.1\r\n", url);
            strcat(buffer, "Host: ");
            strcat(buffer, hostname);
            strcat(buffer, "\r\n");

        }
        else{
            printf("Invalid Command\n");
        }
    }
        /* Open file in Google Chrome */
    // char *args[] = {"google-chrome", "response.html", NULL};
    // execvp(args[0], args);

    return 0;
}
//./b 203.110.245.250 80 http://cse.iitkgp.ac.in/~agupta/networks/index.html
//./b 203.110.245.250 80 http://cse.iitkgp.ac.in/~agupta/networks/1-Introduction.pdf
// ./b 188.184.21.108 80 http://info.cern.ch/hypertext/WWW/TheProject.html
