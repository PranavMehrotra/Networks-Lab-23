/*
    Members:
    Pranav Mehrotra : 20CS10085
    Saransh Sharma  : 20CS30065 
*/


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

#define BUFSIZE 1024
#define MAX_SIZE 500
#define MAX_HEADER_SIZE 256

//struct for header name:value
typedef struct{
    char *name;
    char *value;
}header;

//struct for request
typedef struct{
    char method[7];     //GET/PUT
    char url[BUFSIZE];  //url receieved from user
    header *headers;    //array of headers
    int num_headers;    //total number of headers
    char *body;         //body content
    int body_size;      //number of characters in body 
}request;

//struct for response
typedef struct{
    int status_code;            //200,400,403,404
    char status_message[30];    //status message following code
    header *headers;            //array of headers
    int num_headers;            //number of headers 
    char *body;                 //content receieved
    int body_size;              //number of characters in body
    char content_type[30];      //type of content
}response;

//receive an expression/string in chunks
char *recieve_expr(int newsockfd, int *recv_size){
	char *s;
	int len=0,size = MAX_SIZE,y,chunk=400;
	s = (char *)malloc(sizeof(char)*size);
	if(!s)	return NULL;
    //keep on receiving characters in chunks of 400 and store them in buffer
	while((y=recv(newsockfd,s+len,chunk,0))>0){
        len+=y;
		//update length of message recieved
		while(len+chunk>=size){
            //realloc the array to accomodate more incoming messages
			s = realloc(s,sizeof(char)*(size*=2));
			if(!s)	return NULL;
		}
	}
	if(len==0){
        //No character recieved i.e recv returned <=0
		free(s);
		return NULL;
	}
	s = realloc(s,sizeof(char)*(len+1));
    *recv_size = len;
    return s;
    //return the message read and update the length
}

//send expression in chunks
void send_expr(int newsockfd, char *expr, int size){
	int len=0,y,chunk=MAX_SIZE;
	if(size<chunk){
        //if the size to be sent is less than chunk size send the entire message
		send(newsockfd,expr,size,0);
		return;
	}
	while(1){
        //else break in chunks of size MAX_SIZE
		if(len+chunk>size){
			send(newsockfd,expr+len,size-len,0);
			break;
		}
		y=send(newsockfd,expr+len,chunk,0);
		len+=y;
	}
}

//get file size to be used for content length
long get_file_size(FILE *file) {
    fseek(file, 0, SEEK_END);       //move the file pointer to end of file
    long size = ftell(file);        //tell the current position of file pointer
    rewind(file);                   //move the pointer back to start of the file
    return size;                    //size contains the number of characters in the file
}

//get content type from filename and save it in file pointer for Accept header
void get_content_type(const char *file_name, char *file) {
    //get the extensionby finding position of dot in filename ( filename.extension )
    const char *extension = strrchr(file_name, '.');
    extension++;

    //compare the extension string with different types and return appropriately
    if (strcmp(extension, "html") == 0) {
        strcpy(file,"text/html; charset=UTF-8");
    } else if (strcmp(extension, "pdf") == 0) {
        strcpy(file,"application/pdf");
    } 
    else if (strcmp(extension, "jpeg") == 0) {
        strcpy(file,"image/jpeg");
    }
    else {
        strcpy(file,"text/*");
    } 
}

//to parse http response receieved and store information in appropriate fields
int parse_http_response(char *resp, int response_len, response *parsed_response) {
    
    memset(parsed_response, 0, sizeof(response));               // Initialize the response struct
    const char *header_end = strstr(resp, "\r\n\r\n");          // Split the response into headers and body using delimiter \r\n\r\n
    if (header_end == NULL) {                               
        // Handle error: no header end found
        return 1;
    }
    //find the length of header
    int header_len = header_end - resp + 4;

    //intialise character array to store headers                      
    char *headers = (char *)malloc((header_len + 1)*sizeof(char));

    //copy header_len number of characters from response to header array
    strncpy(headers, resp, header_len);
    headers[header_len] = '\0';

    //the remaining part of response is body
    int body_len = response_len - header_len;

    //body would start from location resp[header_len]
    parsed_response->body =  resp + header_len;
    parsed_response->body_size = body_len;
    
    //print all headers
    printf("Headers: %s\n\n", headers);
    
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
    if (sscanf(header_lines[0], "HTTP/1.1 %d %s",&(parsed_response->status_code),parsed_response->status_message)!=2) {
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

    portno = 80;  //default port number
    char hostname[BUFSIZE];
    char command[20],url[BUFSIZE],buffer[3*BUFSIZE],filename[256];
    char *parse_filename;

    // Polling variables
    struct pollfd pollfd[1];
    int timeout = 3000;
    int poll_ret;
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
        
        //create a socket for communication with server
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("ERROR opening socket");
            exit(1);
        }

        //set server address
        memset((char *) &serv_addr, 0,sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
        serv_addr.sin_port = htons(portno);
        
        //connect to server
        if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
            perror("ERROR connecting");
            close(sockfd);
            continue;
        }

        // Assign the socket to the pollfd structure
        pollfd[0].fd = sockfd;
        pollfd[0].events = POLLIN;
        
        char *filetype=NULL;

        //GET request
        if(strcasecmp(command,"GET")==0){
            //parse out filename by finding last slash
            parse_filename = strrchr(url, '/');
            if (parse_filename == NULL) {
                strcpy(filename,"test");
            }
            else{
                //parse out filetype by finding what follows dot in filename (file.extension)
                parse_filename++;
                filetype = strstr(parse_filename, ".");
                if (filetype == NULL) {
                    strcpy(filename,"test");
                }
                else{
                    // filename = malloc(filetype - parse_filename + 1);
                    strncpy(filename, parse_filename, filetype - parse_filename);
                    filename[filetype - parse_filename] = '\0';
                }
            }

            char type[50];
            if(filetype && strlen(filetype) > 1)
                //if file extension is defined in the url i.e of form 
                //http://cse.iitkgp.ac.in/~agupta/networks/index.html
                get_content_type(filetype, type);
            
            else{

                //if url of form http://cse.iitkgp.ac.in/~agupta/networks/
                strcpy(type, "*/*");
            }
            
            time_t now = time(NULL);    //get current date and time
            now -= 2 * 24 * 60 * 60;    // subtract 2 days

            struct tm *tm = gmtime(&now); //convert the time object to GMT time object
            char if_modified[100];          
            strftime(if_modified, sizeof(if_modified), "%a, %d %b %Y %H:%M:%S GMT", tm);    //copy the attributes of GMT struct in if_modified string of form Sat, 11 Feb 2023 19:33:35 GMT
            
            /* Construct HTTP GET request */
            sprintf(buffer, "GET %s HTTP/1.1\r\n", url);
            strcat(buffer, "Host: ");
            strcat(buffer, hostname);
            strcat(buffer, "\r\n");
            strcat(buffer, "If-Modified-Since: ");
            strcat(buffer, if_modified);
            strcat(buffer, "\r\n");
            strcat(buffer, "Accept-Language: ");
            strcat(buffer, "en-us;q=1,en;q=0.9");
            strcat(buffer, "\r\n");
            strcat(buffer, "Content-Language: ");
            strcat(buffer, "en-us");
            strcat(buffer, "\r\n");
            strcat(buffer, "Accept: ");
            strcat(buffer, type);
            strcat(buffer, "\r\n");
            strcat(buffer, "Connection: close\r\n");
            strcat(buffer, "\r\n");
            strcat(buffer, "\r\n\r\n\r\n");
            //end of HTTP request

            int total = 0;
            int bytesReceived = 0;
            //send in chunks
            send_expr(sockfd, buffer, strlen(buffer));
            
            // Wait for response with a timeout 
            poll_ret = poll(pollfd, 1, timeout);
            if (poll_ret == 0) {
                printf("Connection Timeout. No Response from Server.\n");
                close(sockfd);
                continue;
            }
            if (poll_ret < 0) {
                perror("ERROR in poll");
                close(sockfd);
                continue;
            }
            int resp_size=0;
            char *expr = recieve_expr(sockfd,&resp_size);   //receive the HTTP response in chunks
            if(expr==NULL || resp_size==0){
                printf("No Response\n");
                close(sockfd);
                continue;
            }
            // printf("Response received\n");

            //close the connection after recieving the response
            close(sockfd);
            response resp;

            //parse out the response recieved and store in appropriate format
            if(parse_http_response(expr, resp_size,&resp)){
                printf("Error in parsing the HTTP response\n");
                free(expr);
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
            
            if(resp.status_code==200){
                //success response received
                if(strlen(resp.content_type)>0){
                    FILE *fp;
                    char *file_name = malloc(strlen(resp.content_type) + strlen(filename) + 2);

                    //open file with appropriate file name and extension in write mode
                    strcpy(file_name, filename);
                    strcat(file_name, ".");
                    strcat(file_name, resp.content_type);
                    printf("%s\n", file_name);
                    fp = fopen(file_name, "w");
                    fwrite(resp.body, sizeof(char), resp.body_size, fp); //write the content of file received
                    fclose(fp);

                    //fork a child
                    int pid = fork();
                    if(pid==0){
                        //child process open the file name using xdg-open
                        char *args[] = {"xdg-open", file_name, NULL};

                        //flush out/discard anything in stdout or stderr 
                        freopen("/dev/null", "w", stdout);
                        freopen("/dev/null", "w", stderr);
                        execvp(args[0], args);
                        //error in execvp
                        perror("xdg-open");
                        exit(0);
                    }
                    free(file_name);
                }
            }
            else if(resp.status_code==400 || resp.status_code==403 || resp.status_code==404){
                printf("Error: %d %s\n", resp.status_code, resp.status_message);
                //print the status code and response status message in case of 400, 403, 403 error
            }
            else{
                //print a generic message in case of any other error
                printf("Unknown Error: %d\n", resp.status_code);
            }
            // Deallocate Response memory
            free(expr);
            for(int i=1;i<resp.num_headers;i++){
                free(resp.headers[i].name);
                free(resp.headers[i].value);
            }
            free(resp.headers);
        }
        //PUT request
        else if(strcasecmp(command,"PUT")==0){
            // printf("%s\n", url);
            char content_type[20];
            get_content_type(filename, content_type);
            char file_path[2*BUFSIZE];

            //get the location of file using url (for directory) and filename
            strcpy(file_path,url);
            strcat(file_path,filename);

            char str[10];
            FILE *file;
            int result;
            struct stat st;
            result = stat(filename, &st);
            //stat function : to retrieve information about a file on a Unix-style file system.
            if (result == -1) {
                perror("Error");
                close(sockfd);
                continue;
            }
            else{
                // printf("--%s\n", filename);
                //open file and read the content
                file = fopen(filename, "rb");
                if (file == NULL) {
                    perror("Error");
                    close(sockfd);
                    continue;
                }
            }
            int size = get_file_size(file);
            // printf("%d\n", size);
            char *ba = (char *)malloc(size* sizeof(char));
            sprintf(str, "%d", size);
            int x = fread(ba, size, 1, file);
            
            fclose(file);

            /* Construct HTTP PUT request */
            sprintf(buffer, "PUT %s HTTP/1.1\r\n", file_path);
            strcat(buffer, "Host: ");
            strcat(buffer, hostname);
            strcat(buffer, "\r\n");
            strcat(buffer, "Content-Type: ");
            strcat(buffer, content_type);
            strcat(buffer, "\r\n");
            strcat(buffer, "Content-Length: ");
            strcat(buffer, str);
            strcat(buffer, "\r\n\r\n");
            //PUT request end
            
            send_expr(sockfd, buffer, strlen(buffer));
            send_expr(sockfd, ba, size);
            if(strcasecmp(hostname,"0.0.0.0")==0 || strcasecmp(hostname,"localhost")==0)
                send_expr(sockfd, "\r\n\r\n\r\n", 6);
            free(ba);
            // Wait for response with a timeout
            poll_ret = poll(pollfd, 1, timeout);
            if (poll_ret == 0) {
                printf("Connection Timeout. No Response from Server.\n");
                close(sockfd);
                continue;
            }
            if (poll_ret < 0) {
                perror("ERROR in poll");
                close(sockfd);
                continue;
            }
            int resp_len;
            char *expr = recieve_expr(sockfd,&resp_len);
            if(expr==NULL || resp_len==0){
                printf("No Response\n");
                close(sockfd);
                continue;
            }
            // printf("Response received\n");
            char *header_end = strstr(expr, "\r\n\r\n");
            if(header_end==NULL){
                printf("Invalid Response\n");
                free(expr);
                close(sockfd);
                continue;
            }
            // Print headers
            char *header = (char *)malloc((header_end-expr+1)*sizeof(char));
            strncpy(header, expr, header_end-expr);
            header[header_end-expr] = '\0';
            printf("Headers: %s\n\n", header);
            free(header);
            char *status = strtok(expr, " ");
            status = strtok(NULL, " ");
            char *status_message = strtok(NULL, "\r\n");
            int status_code = atoi(status);
            if(status_code==200){
                //successful file upload
                printf("File uploaded successfully\n");
            }
            else if(status_code==400 || status_code==403 || status_code==404){
                printf("Error: %d %s\n", status_code, status_message);
            }
            else{
                printf("Unknown Error: %d\n", status_code);
            }
            free(expr);
            //close connection
            close(sockfd);
        }
        else{
            printf("Invalid Command\n");
            close(sockfd);
        }
    }

    return 0;
}
// http://cse.iitkgp.ac.in/~agupta/networks/index.html
// http://cse.iitkgp.ac.in/~agupta/networks/1-Introduction.pdf
// http://info.cern.ch/hypertext/WWW/TheProject.html
