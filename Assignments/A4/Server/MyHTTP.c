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

#define MAX_SIZE 500
#define BUFSIZE 1024

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
    char content_type[30];
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



void get_content_type(const char *file_name, char *file) {
    const char *extension = strrchr(file_name, '.');
    extension++;

    if (strcmp(extension, "html") == 0) {
        strcpy(file,"text/html; charset=UTF-8");
    } else if (strcmp(extension, "pdf") == 0) {
        strcpy(file,"application/pdf");
    } 
    else if (strcmp(extension, "jpeg") == 0) {
        strcpy(file,"image/jpeg");
    }
    else {
        strcpy(file,"application/");
        strcat(file,extension);
    } 
}

long get_file_size(FILE *file) {
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    return size;
}

char *recieve_expr(int newsockfd, int *recv_size){
	char *s;
	int len=0,size = MAX_SIZE,y,chunk=400;
	s = (char *)malloc(sizeof(char)*size);
	if(!s)	return NULL;
	while((y=recv(newsockfd,s+len,chunk,0))>0){
        len+=y;
		if(len>6 && strncmp(s+len-6,"\r\n\r\n\r\n",6)==0)	break;
		while(len+chunk>=size){
			s = realloc(s,sizeof(char)*(size*=2));
			if(!s)	return NULL;
		}
	}
	if(len==0){
		free(s);
		return NULL;
	}
	s = realloc(s,sizeof(char)*(len-6+1));
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

int parse_http_request(char *req, int request_len, request *parsed_request) {
    // Initialize the request struct
    memset(parsed_request, 0, sizeof(request));
    // Split the request into headers and body
    const char *header_end = strstr(req, "\r\n\r\n");
    if (header_end == NULL) {
        // Handle error: no header end found
        return 1;
    }
    int header_len = header_end - req + 4;
    char *headers = (char *)malloc((header_len + 1)*sizeof(char));
    strncpy(headers, req, header_len);
    headers[header_len] = '\0';
    int body_len = request_len - header_len;
    parsed_request->body = (char *)malloc(body_len*sizeof(char));
    memcpy(parsed_request->body, req + header_len, body_len);
    parsed_request->body_size = body_len;
    free(req);
    // printf("Headers: %s\n", headers);
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
    if (sscanf(header_lines[0], "%6s %s HTTP/1.1",parsed_request->method,parsed_request->url) != 2) {
        // Handle error: first line not in correct format
        free(headers);
        return 1;
    }
    else{
        char temp_url[BUFSIZE];
        if(sscanf(parsed_request->url,"http://%s",temp_url)==1){
            strcpy(parsed_request->url,temp_url);
        }
    }

    if(strcmp(parsed_request->method,"GET")==0){
        if(parsed_request->body)
            free(parsed_request->body);
        parsed_request->body_size = 0;
    }

    // Allocate memory for the headers array
    parsed_request->headers = (header *)malloc(num_header_lines * sizeof(header));
    parsed_request->num_headers = num_header_lines;

    // Parse each header line into name and value
    for (int i = 1; i < num_header_lines; i++) {
        header *current_header = &parsed_request->headers[i];
        current_header->name = strtok(header_lines[i], ":");
        current_header->value = current_header->name + strlen(current_header->name) + 1;
        while (current_header->value[0] == ' ') current_header->value++;
        current_header->name = strdup(current_header->name);
        current_header->value = strdup(current_header->value);
        if(strcasecmp(current_header->name,"Content-Length")==0){
            parsed_request->body_size = atoi(current_header->value);
            if(parsed_request->body_size > 0 && strcmp(parsed_request->method,"GET")!=0){
                if(parsed_request->body)
                    parsed_request->body = (char *)realloc(parsed_request->body, parsed_request->body_size);
            }
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
            strncpy(parsed_request->content_type, content_type, content_type_end - content_type);
            parsed_request->content_type[content_type_end - content_type] = '\0';
        }
    }

    // Clean up
    free(headers);
    return 0;
}

void return_response(int status, int newsockfd, char *hostname, char *as, char *filename, char* time, char* file_type)
{   
    FILE *file;
    
    char str[10]; int size;
    char buffer[5*BUFSIZE];
    char *ba;
    file = fopen(filename, "rb");

    if(status==200){
        sprintf(buffer, "HTTP/1.1 %d OK\r\n", status);
        
        
        strcat(buffer, "Last-Modified: ");
        strcat(buffer,time);
        strcat(buffer, "\r\n");
        
    }
    if(status == 404)
    {
        sprintf(buffer, "HTTP/1.1 %d Not Found\r\n", status);
      
    }
    
    size = get_file_size(file);
    sprintf(str, "%d", size);
    ba = (char *)malloc(size);
    fread(ba, size, 1, file);
    fclose(file);
    strcat(buffer, "Date: ");
    strcat(buffer, as);
    strcat(buffer, "\r\n");
    strcat(buffer, "Server: ");
    strcat(buffer, hostname);
    strcat(buffer, "\r\n");
    strcat(buffer, "Content-Length: ");
    strcat(buffer, str);
    strcat(buffer, "\r\n");
    strcat(buffer, "Connection: close\r\n");
    strcat(buffer, "Content-Type: ");
    strcat(buffer, file_type);
    strcat(buffer, "\r\n");
    strcat(buffer, "\r\n");
    send_expr(newsockfd, buffer, strlen(buffer));
    send_expr(newsockfd, ba, size);
    free(ba);
    
}


int main(void)
{	
    // get_content_type("a.c");

	int port_num;
	int	sockfd, newsockfd ; /* Socket descriptors */
	int	clilen;
	struct sockaddr_in	cli_addr, serv_addr;

	int i;
	char buf[BUFSIZE];		/* We will use this buffer for communication */

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
		{perror("Cannot create socket\n");
		exit(0);}
    printf("Enter port number: ");
    scanf("%d", &port_num);

	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= INADDR_ANY;
	serv_addr.sin_port		= htons(port_num);

    printf("%s\n", inet_ntoa(serv_addr.sin_addr));

	if (bind(sockfd, (struct sockaddr *) &serv_addr,
					sizeof(serv_addr)) < 0) {
		perror("Unable to bind local address\n");
		exit(0);
	}

	listen(sockfd, 5); 

	while (1) {

		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr,
					&clilen);

		if (newsockfd < 0) {
			perror("Accept error\n");
			exit(0);
		}
        char buffer[5*BUFSIZE],filename[256];
        int req_size = 0;
		char *expr = recieve_expr(newsockfd, &req_size);
		if(!expr){
			break;
		}
        request req;
        if(parse_http_request(expr, req_size,&req)){
            printf("Error in parsing the HTTP request\n");
            continue;
        }
        // Print the request
        printf("Method: %s\n", req.method);
        printf("URL: %s\n", req.url);
        printf("Body size: %d\n", req.body_size);
        printf("Number of headers: %d\n", req.num_headers);
        for (int i = 1; i < req.num_headers; i++) {
            printf("Header %d: %s: %s\n", i, req.headers[i].name, req.headers[i].value);
        }
        printf("Content type: %s\n", req.content_type);
        if(req.body_size > 0)
            printf("\n\nBody: %s\n", req.body);
        if(strcmp(req.method,"GET")==0)
        {
        FILE *file;
        int status =200;
        time_t rawtime;
        struct tm *timeinfo;
        char as[80];

        time(&rawtime);
        timeinfo = gmtime(&rawtime);

        strftime(as, 80, "%a, %d %b %Y %T GMT", timeinfo);
       
        
        // struct tm gmt_time = {0};
        // strptime(str_time, "%a, %d %b %Y %T GMT", &gmt_time);

        // // Calculate difference between two GMT time objects
        // struct tm other_gmt_time = {0};
        // strptime("Sun, 12 Feb 2023 19:33:35 GMT", "%a, %d %b %Y %T GMT", &other_gmt_time);
        // time_t gmt_time_seconds = mktime(&gmt_time);
        // time_t other_gmt_time_seconds = mktime(&other_gmt_time);
        // double difference_in_seconds = difftime(other_gmt_time_seconds, gmt_time_seconds);
        // printf("Difference between two GMT time objects in seconds: %f\n", difference_in_seconds);

        char file_name[256];
        char *parse_filename = strchr(req.url, '/');
        if (parse_filename == NULL) {
            strcpy(file_name,"test.txt");
        }
        else{
            parse_filename++;
            strcpy(file_name, parse_filename);
        }
        int result;
        struct stat st;
        struct tm *gmt;
        char time[50];
        char hostname[256];
        gethostname(hostname, sizeof(hostname));
        char file_type[256];

        result = stat(file_name, &st);
        if (result == -1) {
            strcpy(file_type,"text/html");
            return_response(404, newsockfd, hostname, as, "err_404.html", NULL, file_type);
        }
        else{
            gmt = gmtime(&st.st_mtime);

            strftime(time, sizeof(time), "%a, %d %b %Y %T GMT", gmt);
            file = fopen(file_name, "rb");
            if(file==NULL)
            {
                printf("Error in opening file");
                exit(0);
            }
             char str[10];
   
      
            if(status==200)get_content_type(file_name,file_type);
            return_response(status, newsockfd, hostname, as, file_name, time,file_type);
            
            printf("Sent!\n");
            }
            
        }
        else if(strcmp(req.method, "PUT")==0){
                // printf("%s\n", expr);
            }
        // Deallocate the request
        if(req.body_size > 0)
            free(req.body);
        for (int i = 1; i < req.num_headers; i++) {
            free(req.headers[i].name);
            free(req.headers[i].value);
        }
        free(req.headers);
        close(newsockfd);
	}
	return 0;
}
			

