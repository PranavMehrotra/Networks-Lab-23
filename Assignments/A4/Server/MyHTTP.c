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

#define BUFSIZE 1024
#define MAX_SIZE 500
#define CHUNK_SIZE 400
#define FILE_NAME_SIZE 256
#define SMALL_MSG_SIZE 30
#define TIME_BUF_SIZE 30
#define EXECESS_CHARS_LEN 6

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
    char content_type[SMALL_MSG_SIZE];
}request;

typedef struct{
    int status_code;
    char status_message[SMALL_MSG_SIZE];
    header *headers;
    int num_headers;
    char *body;
    int body_size;
    char content_type[SMALL_MSG_SIZE];
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
	int len=0,size = MAX_SIZE,y,chunk=CHUNK_SIZE;
	s = (char *)malloc(sizeof(char)*size);
	if(!s)	return NULL;
	while((y=recv(newsockfd,s+len,chunk,0))>0){
        len+=y;
		if(len>EXECESS_CHARS_LEN && strncmp(s+len-EXECESS_CHARS_LEN,"\r\n\r\n\r\n",EXECESS_CHARS_LEN)==0)	break;
		while(len+chunk>=size){
			s = realloc(s,sizeof(char)*(size*=2));
			if(!s)	return NULL;
		}
	}
	if(len==0){
		free(s);
		return NULL;
	}
	s = realloc(s,sizeof(char)*(len-EXECESS_CHARS_LEN+1));
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

void return_response(int status, int newsockfd, const char *hostname, const char *current_time, const char *filename, const char* last_modify_time, const char * file_type)
{   
    FILE *file=NULL;
    
    char str[10]; int size;
    char buffer[3*BUFSIZE];
    char *ba=NULL;

    if(status==200){
        sprintf(buffer, "HTTP/1.1 %d OK\r\n", status);
        if(last_modify_time){
            strcat(buffer, "Last-Modified: ");
            strcat(buffer,last_modify_time);
            strcat(buffer, "\r\n");
        }
        
    }
    else if(status == 404)
    {
        sprintf(buffer, "HTTP/1.1 %d Not Found\r\n", status);
      
    }
    else if(status == 400)
    {
        sprintf(buffer, "HTTP/1.1 %d Bad Request\r\n", status);
      
    }
    else if(status == 403)
    {
        sprintf(buffer, "HTTP/1.1 %d Forbidden\r\n", status);
      
    }
    else if(status == 304)
    {
        sprintf(buffer, "HTTP/1.1 %d Not Modified\r\n", status);
      
    }
    else if(status == 500)
    {
        sprintf(buffer, "HTTP/1.1 %d Internal Server Error\r\n", status);
      
    }
    else{
        sprintf(buffer, "HTTP/1.1 %d Unknown Error\r\n", status);
    }
    if(current_time){
        strcat(buffer, "Date: ");
        strcat(buffer, current_time);
        strcat(buffer, "\r\n");
    }
    if(hostname){
        strcat(buffer, "Server: ");
        strcat(buffer, hostname);
        strcat(buffer, "\r\n");
    }
    strcat(buffer, "Connection: close\r\n");
    if(filename)
        file = fopen(filename, "rb");
    if(file){
        size = get_file_size(file);
        sprintf(str, "%d", size);
        ba = (char *)malloc(size);
        fread(ba, size, 1, file);
        fclose(file);
        strcat(buffer, "Content-Length: ");
        strcat(buffer, str);
        strcat(buffer, "\r\n");
        if(file_type){
            strcat(buffer, "Content-Type: ");
            strcat(buffer, file_type);
            strcat(buffer, "\r\n");
        }
    }
    strcat(buffer, "\r\n");
    send_expr(newsockfd, buffer, strlen(buffer));
    if(ba){
        send_expr(newsockfd, ba, size);
        free(ba);
    }
}


int main(void)
{

	int port_num;
	int	sockfd, newsockfd ; /* Socket descriptors */
	int	clilen;
	struct sockaddr_in	cli_addr, serv_addr;

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
    char hostname[FILE_NAME_SIZE];
    gethostname(hostname, sizeof(hostname));
	while (1) {

		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr,
					&clilen);

		if (newsockfd < 0) {
			perror("Accept error\n");
			exit(0);
		}
        char filename[FILE_NAME_SIZE];
        int req_size = 0;
		char *expr = recieve_expr(newsockfd, &req_size);
		if(!expr){
			printf("Error in recieving the HTTP request\n");
            return_response(400, newsockfd, hostname, NULL, "err_400.html", NULL, "text/html");
            continue;
		}
        request req;
        if(parse_http_request(expr, req_size,&req)){
            printf("Error in parsing the HTTP request\n");
            return_response(400, newsockfd, hostname, NULL, "err_400.html", NULL, "text/html");
            continue;
        }
        // Print the request
        printf("Method: %s\n", req.method);
        printf("URL: %s\n", req.url);
        printf("Body size: %d\n", req.body_size);
        printf("Number of headers: %d\n", req.num_headers-1);
        for (int i = 1; i < req.num_headers; i++) {
            printf("%s: %s\n", req.headers[i].name, req.headers[i].value);
        }
        // printf("Content type: %s\n", req.content_type);


        FILE *access;
        time_t rawtime;
        struct tm *timeinfo;
        char current_time[TIME_BUF_SIZE];
        char date[TIME_BUF_SIZE];
        char t[TIME_BUF_SIZE];

        time(&rawtime);
        timeinfo = gmtime(&rawtime);

        strftime(current_time, TIME_BUF_SIZE, "%a, %d %b %Y %T GMT", timeinfo);
        
        strftime(date, TIME_BUF_SIZE, "%d%m%y", timeinfo);
        strftime(t, TIME_BUF_SIZE, "%H%M%S", timeinfo);

        char input[2*BUFSIZE];
        sprintf(input, "%s:%s:%s:%d:%s:%s\n",date, t,inet_ntoa(cli_addr.sin_addr),ntohs(cli_addr.sin_port),req.method, req.url);
        // printf("%s", input);
        access = fopen("AccessLog.txt", "a+");
        if(access == NULL)
            perror("Error opening Access Log file");
        else{
            fprintf(access, "%s", input);
            fclose(access);
        }

        if(strcmp(req.method,"GET")==0){
        FILE *file;
        int status =200;
        
        // struct tm gmt_time = {0};
        // strptime(str_time, "%a, %d %b %Y %T GMT", &gmt_time);

        // // Calculate difference between two GMT time objects
        // struct tm other_gmt_time = {0};
        // strptime("Sun, 12 Feb 2023 19:33:35 GMT", "%a, %d %b %Y %T GMT", &other_gmt_time);
        // time_t gmt_time_seconds = mktime(&gmt_time);
        // time_t other_gmt_time_seconds = mktime(&other_gmt_time);
        // double difference_in_seconds = difftime(other_gmt_time_seconds, gmt_time_seconds);
        // printf("Difference between two GMT time objects in seconds: %f\n", difference_in_seconds);

        char file_name[FILE_NAME_SIZE];
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
        char last_modify_time[TIME_BUF_SIZE];
        char file_type[FILE_NAME_SIZE];

        result = stat(file_name, &st);
        if (result == -1) {
            strcpy(file_type,"text/html");
            return_response(404, newsockfd, hostname, current_time, "err_404.html", NULL, file_type);
        }
        else{
            gmt = gmtime(&st.st_mtime);

            strftime(last_modify_time, sizeof(last_modify_time), "%a, %d %b %Y %T GMT", gmt);
            

            struct tm is_modified = {0};
            for (int i = 1; i < req.num_headers; i++) {
                if(strcmp(req.headers[i].name, "If-Modified-Since")==0)
                {
                    char month[5];
                    sscanf(req.headers[i].value, "%*3s, %d %3s %d %d:%d:%d GMT", &is_modified.tm_mday, month, &is_modified.tm_year, &is_modified.tm_hour, &is_modified.tm_min, &is_modified.tm_sec);
                    is_modified.tm_year -= 1900;
                    if (strcmp(month, "Jan") == 0) is_modified.tm_mon = 0;
                    else if (strcmp(month, "Feb") == 0) is_modified.tm_mon = 1;
                    else if (strcmp(month, "Mar") == 0) is_modified.tm_mon = 2;
                    else if (strcmp(month, "Apr") == 0) is_modified.tm_mon = 3;
                    else if (strcmp(month, "May") == 0) is_modified.tm_mon = 4;
                    else if (strcmp(month, "Jun") == 0) is_modified.tm_mon = 5;
                    else if (strcmp(month, "Jul") == 0) is_modified.tm_mon = 6;
                    else if (strcmp(month, "Aug") == 0) is_modified.tm_mon = 7;
                    else if (strcmp(month, "Sep") == 0) is_modified.tm_mon = 8;
                    else if (strcmp(month, "Oct") == 0) is_modified.tm_mon = 9;
                    else if (strcmp(month, "Nov") == 0) is_modified.tm_mon = 10;
                    else if (strcmp(month, "Dec") == 0) is_modified.tm_mon = 11;
                    else is_modified.tm_mon = -1;
                }
            }
            
        
            time_t last_modify_seconds = mktime(gmt);
            time_t is_modified_seconds = mktime(&is_modified);
            double difference_in_seconds = difftime(last_modify_seconds, is_modified_seconds);
            // printf("Difference between two GMT time objects in seconds: %f\n", difference_in_seconds);
            if(difference_in_seconds<0){
                status = 304;
                return_response(status, newsockfd, hostname, current_time, NULL, NULL, NULL);
            }
            else{
                file = fopen(file_name, "rb");
                if(file==NULL){
                    perror("Error in opening file");
                    // strcpy(file_type,"text/html");
                    return_response(404, newsockfd, hostname, current_time, "err_404.html", NULL, "text/html");
                }
                else if(status==200){
                    get_content_type(file_name,file_type);
                    return_response(status, newsockfd, hostname, current_time, file_name, last_modify_time,file_type);
                }
                }
            }
            
        }
        else if(strcmp(req.method, "PUT")==0){
            int status = 200;
            FILE *file;
            char file_type[FILE_NAME_SIZE];
            file = fopen((req.url)+1, "wb");
            if(file==NULL)
            {
                perror("Error in opening file");
                // strcpy(file_type,"text/html");
                return_response(404, newsockfd, hostname, current_time, "err_404.html", NULL, "text/html");
            }
            else{
                if(fwrite(req.body, 1, req.body_size, file)!=req.body_size){
                    perror("Error in writing to file");
                    status = 500;
                }
                fclose(file);
                return_response(status, newsockfd, hostname, current_time, NULL, NULL,NULL);
            }
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
			

