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


char *get_content_type(const char *file_name) {
    const char *extension = strrchr(file_name, '.');
    extension++;
    char *file = (char*)malloc(1000*sizeof(char));
    printf("%s\n", extension);

    if (strcmp(extension, "html") == 0) {
        return "text/html; charset=UTF-8";
    } else if (strcmp(extension, "pdf") == 0) {
        return "application/pdf";
    } 
    else if (strcmp(extension, "jpeg") == 0) {
        return "image/jpeg";
    }
    else {
        strcat(file,"application/");
        strcat(file,extension);
        printf("%s\n",file);
        return file;
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
					&clilen) ;
		printf("accepted\n");

		if (newsockfd < 0) {
			perror("Accept error\n");
			exit(0);
		}
		while(1){
        char command[20],buffer[5*BUFSIZE],filename[256];
		int length = 0;
		char *expr = recieve_expr(newsockfd, &length);
		if(!expr){
			break;
		}
        char *first_line_end = strstr(expr, "\r\n");
        char *first_line = malloc(first_line_end - expr + 1);
        strncpy(first_line, expr, first_line_end - expr);
        first_line[first_line_end - expr] = '\0';
        char method[10], url[256], version[10];
        sscanf(first_line, "%s %s %s", method, url, version);
        printf("%s %s %s\n", method, url, version);
        free(first_line);
		// printf("\n%s", expr);
        if(strcmp(method,"GET")==0)
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
        char *parse_filename = strrchr(url, '/');
        if (parse_filename == NULL) {
            strcpy(file_name,"test.txt");
        }
        else{
            parse_filename++;
            strcpy(file_name, parse_filename);
        }
        printf("\n\t\t%s\n\n\n\n", file_name);
        int result;
        struct stat st;
        struct tm *gmt;
        char time[50];

        result = stat(file_name, &st);
        if (result == -1) {
            
            file = fopen("err_404.html", "rb");
            status=404;
            
        }
        else{
        gmt = gmtime(&st.st_mtime);

        strftime(time, sizeof(time), "%a, %d %b %Y %T GMT", gmt);
        file = fopen(file_name, "rb");
        }
    
        char hostname[256];
        int a;
        char str[10];
       

        int size = get_file_size(file);
        char *ba = (char *)malloc(size);
        sprintf(str, "%d", size);
        fread(ba, size, 1, file);
        fclose(file);

    // char *mime_type = get_file_mime_type(file_name);

    // fprintf(out, "HTTP/1.1 200 OK\r\n");
    // fprintf(out, "Content-Type: %s\r\n", mime_type);
    // fprintf(out, "Content-Length: %ld\r\n", size);
    // fprintf(out, "\r\n");
    
        a = gethostname(hostname, sizeof(hostname));
        if(status==200){
        sprintf(buffer, "HTTP/1.1 %d OK\r\n", status);
        strcat(buffer, "Date: ");
        strcat(buffer, as);
        strcat(buffer, "\r\n");
        strcat(buffer, "Server: ");
        strcat(buffer, hostname);
        strcat(buffer, "\r\n");
        strcat(buffer, "Last-Modified: ");
        strcat(buffer,time);
        strcat(buffer, "\r\n");
        strcat(buffer, "Content-Length: ");
        strcat(buffer, str);
        strcat(buffer, "\r\n");
        strcat(buffer, "Connection: close\r\n");
        strcat(buffer, "Content-Type: ");
        strcat(buffer, get_content_type(file_name));
        strcat(buffer, "\r\n");
        strcat(buffer, "\r\n");
        // strcat(buffer,ba);
        }
        else if(status == 404)
        {sprintf(buffer, "HTTP/1.1 %d Not Found\r\n", status);
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
        strcat(buffer, get_content_type("err_404.html"));
        strcat(buffer, "\r\n");
        strcat(buffer, "\r\n");
        // strcat(buffer,ba);

        }
        printf("%s\n", buffer);
        send_expr(newsockfd, buffer, strlen(buffer));
        send_expr(newsockfd, ba, size);
        printf("Sent!\n");
        }
		else if(strcmp(method, "PUT")==0){
            printf("%s\n", expr);
        }
        
        close(newsockfd);
		}
	}
	return 0;
}
			

