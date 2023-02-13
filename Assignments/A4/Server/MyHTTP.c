/*
    Members:
    Pranav Mehrotra : 20CS10085
    Saransh Sharma  : 20CS30065 
*/

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

extern int errno;

#define BUFSIZE 1024
#define MAX_SIZE 500
#define CHUNK_SIZE 400
#define FILE_NAME_SIZE 256
#define SMALL_MSG_SIZE 30
#define TIME_BUF_SIZE 30
#define EXCESS_CHARS_LEN 6

//header struct name:value
typedef struct{
    char *name;
    char *value;
}header;

//struct to store HTTP request
typedef struct{
    char method[7];                         //GET/PUT
    char url[BUFSIZE];                      //url received from user
    header *headers;                        //array of headers
    int num_headers;                        //total number of headers recieved
    char *body;                             //content of body
    int body_size;                          //number of characters in body
    char content_type[SMALL_MSG_SIZE];      //content type of the file
}request;

//struct to store HTTP response
typedef struct{
    int status_code;                        //200/400/404/403
    char status_message[SMALL_MSG_SIZE];    //status message
    header *headers;                        // array of headers
    int num_headers;                        //total number of headers
    char *body;                             //content of body
    int body_size;                          //number of characters in body
    char content_type[SMALL_MSG_SIZE];      //content type of file
}response;


//get content type using the file name provided
void get_content_type(const char *file_name, char *file) {
    const char *extension = strrchr(file_name, '.');
    extension++;

    //extract the extension from filename
    if (strcmp(extension, "html") == 0) {
        strcpy(file,"text/html; charset=UTF-8");
    } else if (strcmp(extension, "pdf") == 0) {
        strcpy(file,"application/pdf");
    } 
    else if (strcmp(extension, "jpeg") == 0) {
        strcpy(file,"image/jpeg");
    }
    else if(strcmp(extension,"txt")==0){
        strcpy(file,"text/plain");
    }
    else {
        strcpy(file,"application/");
        strcat(file,extension);
    } 
}

//get number of characters in file
long get_file_size(FILE *file) {
    fseek(file, 0, SEEK_END);   //move the file pointer to the end of file
    long size = ftell(file);    //tell the position of pointer
    rewind(file);               //move the pointer back to start
    return size;                //return the number of characters 
}

//receieve an expression/string in chunks
char *recieve_expr(int newsockfd, int *recv_size){
	char *s;
	int len=0,size = MAX_SIZE,y,chunk=CHUNK_SIZE;
	s = (char *)malloc(sizeof(char)*size);
	if(!s)	return NULL;

    //keep on receieving chunks of size 400 and store in buffer of size 500 until there are no more characters left to be read
	while((y=recv(newsockfd,s+len,chunk,0))>0){
        len+=y;
		if(len>EXCESS_CHARS_LEN && strncmp(s+len-EXCESS_CHARS_LEN,"\r\n\r\n\r\n",EXCESS_CHARS_LEN)==0)	break;
		while(len+chunk>=size){
			s = realloc(s,sizeof(char)*(size*=2));
			if(!s)	return NULL;
		}
	}
	if(len==0){
		free(s);
		return NULL;
	}
    //resize the buffer to accomodate more characters
	s = realloc(s,sizeof(char)*(len-EXCESS_CHARS_LEN+1));
    *recv_size = len;
    return s;
}

//send an expression/string in chunks
int send_expr(int newsockfd, char *expr, int size){
	int len=0,y,chunk=MAX_SIZE;
	if(size<chunk){
		if(send(newsockfd,expr,size,0)<0)   return 1;
		return 0;
	}
	while(1){
        //break the message in chunks and keep on sending until all chracters are sent
		if(len+chunk>size){
            
			if(send(newsockfd,expr+len,size-len,0)<0)  return 1;
			break;
		}
		if((y=send(newsockfd,expr+len,chunk,0))<0)  return 1;
		len+=y;
	}
    return 0;
}

//parse the http request recieved from client and extract appropriate information
int parse_http_request(char *req, int request_len, request *parsed_request) {
    
    memset(parsed_request, 0, sizeof(request));         // Initialize the request struct
    
    const char *header_end = strstr(req, "\r\n\r\n");   // Split the request into headers and body
    if (header_end == NULL) {
        // Handle error: no header end found
        return 1;
    }
    //find the length of header
    int header_len = header_end - req + 4;

    //intialise character array to store headers 
    char *headers = (char *)malloc((header_len + 1)*sizeof(char));
    
     //copy header_len number of characters from response to header array
    strncpy(headers, req, header_len);
    headers[header_len] = '\0';
    
    //the remaining part of response is body
    int body_len = request_len - header_len;
    
    // parsed_request->body = (char *)malloc(body_len*sizeof(char));
    //body would start from location resp[header_len]
    parsed_request->body= req + header_len;
    parsed_request->body_size = body_len;
    // free(req);
    // printf("Headers: %s\n", headers);
    // Split the headers into individual lines
    char *header_lines[32];
    int num_header_lines = 0;
     //extract one header using delimiter \r\n
    char *header_line = strtok(headers, "\r\n");
    while (header_line != NULL && num_header_lines < 32) {
        //header_lines[0] = first header and so on
        header_lines[num_header_lines++] = header_line;

        //extract another header
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
        //extract the url provided
        char temp_url[BUFSIZE];
        if(sscanf(parsed_request->url,"http://%s",temp_url)==1){
            strcpy(parsed_request->url,temp_url);
        }
    }

    if(strcmp(parsed_request->method,"GET")==0){
        //GET request receievd so no body content
        free(req);
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
        }
        //case insensitive match
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

//function to return response depending upon status code
void return_response(int status, int newsockfd, const char *hostname, const char *current_time, const char *filename, const char* last_modify_time, const char * file_type)
{   
    FILE *file=NULL;
    char str[10]; int size;
    char buffer[3*BUFSIZE];
    char *ba=NULL; //store content of a file

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
    strcat(buffer, "Cache-control: ");
    strcat(buffer, "no-store");
    strcat(buffer, "\r\n");

    time_t now = time(NULL);    //get current date and time
    now += 3 * 24 * 60 * 60;  // add 3 days

    struct tm *tm = gmtime(&now); //convert to GMT format
    char expires[100];

    //copy the attributes of GMT struct in if_modified string of form Sat, 11 Feb 2023 19:33:35 GMT
    strftime(expires, sizeof(expires), "%a, %d %b %Y %H:%M:%S GMT", tm);
    if(expires){
        strcat(buffer, "Expires: ");
        strcat(buffer, expires);
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
    //ask for port number to bind to server
    scanf("%d", &port_num);

    //server address
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= INADDR_ANY;
	serv_addr.sin_port		= htons(port_num);

    printf("Server IP: %s\n", inet_ntoa(serv_addr.sin_addr));

    //bind the addres to server
	if (bind(sockfd, (struct sockaddr *) &serv_addr,
					sizeof(serv_addr)) < 0) {
		perror("Unable to bind local address\n");
		exit(0);
	}

	listen(sockfd, 5); 
    char hostname[FILE_NAME_SIZE];
    //get the hostname of server device to send in response
    gethostname(hostname, sizeof(hostname));
	while (1) {
		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr,
					&clilen);

		if (newsockfd < 0) {
			perror("Accept error\n");
			continue;
		}
        // Concurrent Server
        if(fork()==0){
            close(sockfd);
            char filename[FILE_NAME_SIZE];
            int req_size = 0;

            //receive the expression in chunks
            char *expr = recieve_expr(newsockfd, &req_size);
            if(!expr){
                printf("Error in recieving the HTTP request\n");
                return_response(400, newsockfd, hostname, NULL, "err_400.html", NULL, "text/html");
                continue;
            }
            request req;
            //parse http request and store in appropriate format
            if(parse_http_request(expr, req_size,&req)){
                printf("Error in parsing the HTTP request\n");
                return_response(400, newsockfd, hostname, NULL, "err_400.html", NULL, "text/html");
                free(expr);
                continue;
            }
            // Print the request received
            printf("Method: %s\n", req.method);
            printf("URL: %s\n", req.url);
            printf("Body size: %d\n", req.body_size);
            printf("Number of headers: %d\n", req.num_headers-1);
            for (int i = 1; i < req.num_headers; i++) {
                printf("%s: %s\n", req.headers[i].name, req.headers[i].value);
            }
            printf("\n");
            // printf("Content type: %s\n", req.content_type);
            FILE *access;
            time_t rawtime;
            struct tm *timeinfo;
            char current_time[TIME_BUF_SIZE];
            char date[TIME_BUF_SIZE];
            char t[TIME_BUF_SIZE];

            //get current time i.e. time when response is receievd
            time(&rawtime);
            timeinfo = gmtime(&rawtime); //convert to GMT format

            strftime(current_time, TIME_BUF_SIZE, "%a, %d %b %Y %T GMT", timeinfo);
            
            //extract the date and time from GMT object to store in access log
            strftime(date, TIME_BUF_SIZE, "%d%m%y", timeinfo);
            strftime(t, TIME_BUF_SIZE, "%H%M%S", timeinfo);

            char input[2*BUFSIZE];
            sprintf(input, "%s:%s:%s:%d:%s:%s\n",date, t,inet_ntoa(cli_addr.sin_addr),ntohs(cli_addr.sin_port),req.method, req.url);
            // printf("%s", input);
            //append the string in access log
            access = fopen("AccessLog.txt", "a+");
            if(access == NULL)
                perror("Error opening Access Log file");
            else{
                fprintf(access, "%s", input);
                fclose(access);
            }

            if(strcmp(req.method,"GET")==0){
                //GET request recieved
                FILE *file;
                int status =200;

                //extract the file name requested
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

                //extract the last modified date of the file
                struct stat st;
                struct tm *gmt;
                char last_modify_time[TIME_BUF_SIZE];
                char file_type[FILE_NAME_SIZE];

                errno = 0;
                result = stat(file_name, &st);
                if (result == -1) {
                    //the file requested can't be opened
                    if(errno==EACCES){
                        //protected file
                        strcpy(file_type,"text/html");
                        return_response(404, newsockfd, hostname, current_time, "err_403.html", NULL, file_type);
                    }
                    else {
                        //file does not exist
                        strcpy(file_type,"text/html");
                        return_response(404, newsockfd, hostname, current_time, "err_404.html", NULL, file_type);
                    }
                }
                else{
                    //get last modified info from file
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
                    //convert the string if modified since received from client in form Sat, 11 Feb 2023 19:33:35 GMT to GMT object
                
                    double difference_in_seconds=1;
                    if(is_modified.tm_mday!=0){
                        //convert GMT objects to time objects to calculate difference in time
                        time_t last_modify_seconds = mktime(gmt);
                        time_t is_modified_seconds = mktime(&is_modified);
                        difference_in_seconds =  difftime(last_modify_seconds, is_modified_seconds);
                    }
                    
                    // printf("Difference between two GMT time objects in seconds: %f\n", difference_in_seconds);
                    if(difference_in_seconds<0){
                        //file was modified before "if modified since"
                        status = 304;
                        return_response(status, newsockfd, hostname, current_time, NULL, NULL, NULL);
                    }
                    else{
                        //if the file is modified recently
                        errno = 0;
                        file = fopen(file_name, "rb");
                        if(file==NULL){
                            if(errno==EACCES){
                                //protected file
                                strcpy(file_type,"text/html");
                                return_response(404, newsockfd, hostname, current_time, "err_403.html", NULL, file_type);
                            }
                            else{
                                //file doesn't exist
                                perror("Error in opening file");
                                return_response(404, newsockfd, hostname, current_time, "err_404.html", NULL, "text/html");
                            }
                        }
                        else if(status==200){
                            //successfully opened file
                            fclose(file);
                            //get content type from filename and send the response 
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
                //open the directory+filename provided
                file = fopen((req.url)+1, "wb");
                if(file==NULL)
                {
                    perror("Error in opening file");
                    // strcpy(file_type,"text/html");
                    return_response(404, newsockfd, hostname, current_time, "err_404.html", NULL, "text/html");
                }
                else{
                    if(fwrite(req.body, 1, req.body_size, file)!=req.body_size){
                        //write the content received from client to the file
                        perror("Error in writing to file");
                        status = 500;
                    }
                    fclose(file);
                    //return the response
                    return_response(status, newsockfd, hostname, current_time, NULL, NULL,NULL);
                }
            }
            else{
                //Bad request
                return_response(400, newsockfd, hostname, NULL, "err_400.html", NULL, "text/html");
            }
            // Deallocate the request
            if(req.body_size > 0)
                free(expr);
            for (int i = 1; i < req.num_headers; i++) {
                free(req.headers[i].name);
                free(req.headers[i].value);
            }
            free(req.headers);
            close(newsockfd);
            exit(0);
        }
        //connection closed
        close(newsockfd);
	}
	return 0;
}
			

