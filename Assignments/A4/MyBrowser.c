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

#define BUFSIZE 1024
#define MAX_SIZE 500
#define MAX_HEADER_SIZE 256

typedef struct{
    char *name;
    char *value;
}header;

typedef struct{
    char method[7];
    char url[BUFSIZE];
    header *headers;
    int num_headers;
    char *body;
    int body_size;
}request;

typedef struct{
    int status_code;
    char status_message[30];
    header *headers;
    int num_headers;
    char *body;
    int body_size;
    char content_type[30];
}response;


char *recieve_expr(int newsockfd, int *recv_size){
	char *s;
	int len=0,size = MAX_SIZE,y,chunk=400;
	s = (char *)malloc(sizeof(char)*size);
	if(!s)	return NULL;
	while((y=recv(newsockfd,s+len,chunk,0))>0){
        len+=y;
		// if(s[len-1]=='\0')	break;
		while(len+chunk>=size){
			s = realloc(s,sizeof(char)*(size*=2));
			if(!s)	return NULL;
		}
	}
	if(len==0){
		free(s);
		return NULL;
	}
	s = realloc(s,sizeof(char)*(len+1));
    *recv_size = len;
    return s;
}

void send_expr(int newsockfd, char *expr, int size){
	int len=0,y,chunk=MAX_SIZE;
	if(size<chunk){
		send(newsockfd,expr,size,0);
		return;
	}
	while(1){
		if(len+chunk>size){
			send(newsockfd,expr+len,size-len,0);
			break;
		}
		y=send(newsockfd,expr+len,chunk,0);
		len+=y;
	}
}

long get_file_size(FILE *file) {
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    return size;
}

int parse_http_response(char *resp, int response_len, response *parsed_response) {
    // Initialize the response struct
    memset(parsed_response, 0, sizeof(response));
    // Split the response into headers and body
    const char *header_end = strstr(resp, "\r\n\r\n");
    if (header_end == NULL) {
        // Handle error: no header end found
        return 1;
    }
    size_t header_len = header_end - resp + 4;
    char *headers = (char *)malloc((header_len + 1)*sizeof(char));
    strncpy(headers, resp, header_len);
    headers[header_len] = '\0';
    size_t body_len = response_len - header_len;
    parsed_response->body = (char *)malloc(body_len*sizeof(char));
    memcpy(parsed_response->body, resp + header_len, body_len);
    parsed_response->body_size = body_len;
    free(resp);
    printf("Headers: %s\n", headers);
    // Split the headers into individual lines
    char *header_lines[32];
    int num_header_lines = 0;
    char *header_line = strtok(headers, "\r\n");
    while (header_line != NULL && num_header_lines < 32) {
        header_lines[num_header_lines++] = header_line;
        header_line = strtok(NULL, "\r\n");
        if(header_line==NULL) break;
    }
    // Parse the first line to get the HTTP status code
    if (sscanf(header_lines[0], "HTTP/1.1 %d %s",&(parsed_response->status_code),parsed_response->status_message) < 1) {
        // Handle error: first line not in correct format
        free(headers);
        return 1;
    }

    // Allocate memory for the headers array
    parsed_response->headers = (header *)malloc(num_header_lines * sizeof(header));
    parsed_response->num_headers = num_header_lines;

    // Parse each header line into name and value
    for (int i = 1; i < num_header_lines; i++) {
        header *current_header = &parsed_response->headers[i];
        current_header->name = strtok(header_lines[i], ":");
        current_header->value = current_header->name + strlen(current_header->name) + 1;
        while (current_header->value[0] == ' ') current_header->value++;
        current_header->name = strdup(current_header->name);
        current_header->value = strdup(current_header->value);
        if(strcasecmp(current_header->name,"Content-Length")==0){
            parsed_response->body_size = atoi(current_header->value);
        }
        else if(strcasecmp(current_header->name,"Content-Type")==0){
            // Parse the content_type to get the content type, with delimeter '/'
            char *content_type = strstr(current_header->value, "/");
            content_type++;
            // Take until ';'
            char* content_type_end = strstr(content_type, ";");
            if(content_type_end == NULL){
                // Take until '\r\n'
                content_type_end = content_type + strlen(content_type);
            }
            strncpy(parsed_response->content_type, content_type, content_type_end - content_type);
            parsed_response->content_type[content_type_end - content_type] = '\0';
        }
    }

    // Clean up
    free(headers);
    return 0;
}

int main() {
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    portno = 80;
    char hostname[BUFSIZE];
    char command[20],url[BUFSIZE],buffer[3*BUFSIZE],filename[256];
    char *parse_filename;
    while(1){
        
        printf("MyOwnBrowser> ");
        scanf("%s",command);
        if(strcasecmp(command,"QUIT")==0)   break;
        if(strcasecmp(command,"GET")!=0 && strcasecmp(command,"PUT")!=0){
            printf("Please enter a valid command\n");
            continue;
        }
        scanf("%s",url);
        if(strcasecmp(command,"PUT")==0)    scanf("%s",filename);
        // Parse the URL string to extract the hostname (domain name)
        if (sscanf(url, "http://%1000[^:]:%d", hostname, &portno) == 2) {
            // Port is specified in the URL
            sscanf(url, "http://%255[^/:]", hostname);
            char temp_url[BUFSIZE];
            sscanf(url, "http://%1000[^:]", temp_url);
            strcpy(url,"http://");
            strcat(url,temp_url);
            // printf("URL: %s\n Port: %d \n Host: %s\n",url,portno,hostname);
        } else if (sscanf(url, "http://%255[^/]", hostname) == 1) {
            // Port is not specified, use the default
        } else {
            // URL format is invalid
            printf("Please enter a valid HTTP URL\n");
            continue;
        }
        // printf("%s\n", hostname);
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

                time_t now = time(NULL);
            now -= 2 * 24 * 60 * 60;  // subtract 2 days

            struct tm *tm = gmtime(&now);
            char cs[100];
            strftime(cs, sizeof(cs), "%a, %d %b %Y %H:%M:%S GMT", tm);
            printf("%s\n", cs);

            printf("%s\n", cs);
            /* Construct HTTP GET request */
            sprintf(buffer, "GET %s HTTP/1.1\r\n", url);
            strcat(buffer, "Host: ");
            strcat(buffer, hostname);
            strcat(buffer, "If-Modified-Since: ");
            strcat(buffer, "\r\n");
            strcat(buffer, "02");
            strcat(buffer, "\r\n");
            strcat(buffer, "Accept: */*\r\n");
            strcat(buffer, "Connection: close\r\n");
            strcat(buffer, "\r\n");
            strcat(buffer, "\r\n\r\n\r\n");
            int total = 0;
            int bytesReceived = 0;
            send_expr(sockfd, buffer, strlen(buffer));
            int resp_size=0;
            char *s = recieve_expr(sockfd,&resp_size);
            if(s==NULL || resp_size==0){
                printf("No Response\n");
                continue;
            }
            printf("Response received\n");
            close(sockfd);
            response resp;
            if(parse_http_response(s, resp_size,&resp)){
                printf("Error in parsing the HTTP response\n");
                continue;
            }

            // Print the response
            // printf("Number of headers: %d\n",resp.num_headers);
            // printf("HTTP/1.1 %d\r\n", resp.status_code);
            // for (int i = 1; i < resp.num_headers; i++) {
            //     printf("%s:%s\r\n", resp.headers[i].name, resp.headers[i].value);
            // }
            // printf("\r\nContent-Type: %s\r\n", resp.content_type);
            // printf("%d\n\n", resp.body_size);

            FILE *fp;
            char *filename = malloc(strlen(resp.content_type) + strlen(filename) + 2);
            strcpy(filename, filename);
            strcat(filename, ".");
            strcat(filename, resp.content_type);
            // printf("%s\n", filename);
            fp = fopen(filename, "w");
            fwrite(resp.body, sizeof(char), resp.body_size, fp);
            fclose(fp);

            // Deallocate Response memory
            free(resp.body);
            for(int i=1;i<resp.num_headers;i++){
                free(resp.headers[i].name);
                free(resp.headers[i].value);
            }
            free(resp.headers);

            int pid = fork();
            if(pid==0){
                char *args[] = {"xdg-open", filename, NULL};
                freopen("/dev/null", "w", stdout);
                freopen("/dev/null", "w", stderr);
                execvp(args[0], args);
                perror("xdg-open");
                exit(0);
            }
            free(filename);
        }
        else if(strcasecmp(command,"PUT")==0){
            printf("hello\n");
            printf("%s\n", url);
            /* Construct HTTP PUT request */
            sprintf(buffer, "PUT %s HTTP/1.1\r\n", url);
            strcat(buffer, "Host: ");
            strcat(buffer, hostname);
            strcat(buffer, "\r\n");
            strcat(buffer, "\r\n");
        
            int a;
            char str[10];
        
            FILE *file;
            int result;
             struct stat st;
            result = stat(filename, &st);
            if (result == -1) {
                printf("error\n");
                file = fopen("err_404.html", "rb");
                
                
            }
            else{
                printf("--%s\n", filename);
                file = fopen(filename, "rb");
            }
            int size = get_file_size(file);
            printf("%d\n", size);
            char *ba = (char *)malloc(size* sizeof(char));
            sprintf(str, "%d", size);
            size_t x = fread(ba, size, 1, file);
            // printf("**%lu\n", x);
            // printf("%s\n", ba);
            
            fclose(file);

            printf("%s\n", buffer);
            
            send_expr(sockfd, buffer, strlen(buffer));
            send_expr(sockfd, ba, size);
            send_expr(sockfd, "\r\n\r\n\r\n", strlen("\r\n\r\n\r\n"));
            printf("Sent!\n");


        }
        else{
            printf("Invalid Command\n");
        }
    }

    return 0;
}
//./b 203.110.245.250 80 http://cse.iitkgp.ac.in/~agupta/networks/index.html
//./b 203.110.245.250 80 http://cse.iitkgp.ac.in/~agupta/networks/1-Introduction.pdf
// ./b 188.184.21.108 80 http://info.cern.ch/hypertext/WWW/TheProject.html
