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

long get_file_size(const char *file_name) {
    FILE *file = fopen(file_name, "rb");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fclose(file);

    return size;
}

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
    get_content_type("a.c");

	int port_num = 2000;
	int	sockfd, newsockfd ; /* Socket descriptors */
	int	clilen;
	struct sockaddr_in	cli_addr, serv_addr;

	int i;
	char buf[BUFSIZE];		/* We will use this buffer for communication */

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
		{perror("Cannot create socket\n");
		exit(0);}


	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= INADDR_ANY;
	serv_addr.sin_port		= htons(80);

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
        char command[20],url[BUFSIZE],buffer[2*BUFSIZE],filename[256];
		int length = 0;
		char *expr = recieve_expr(newsockfd, &length);
		if(!expr){
			break;
		}
		printf("\n%s", expr);
        if(expr[0]=='G' && expr[1]=='E', expr[2]=='T')
        {
        FILE *file;
        int status =200;
        time_t rawtime;
        struct tm *timeinfo;
        char as[80];

        time(&rawtime);
        timeinfo = gmtime(&rawtime);

        strftime(as, 80, "%a, %d %b %Y %T GMT", timeinfo);
       
        char file_name[100];
        int i, j = 0, length = strlen(expr);

        for (i = 4; i < length; i++) {
            if (expr[i] == '/') {
                j = 0;
                i++;
            }
            if (expr[i] == ' ') {
                break;
            }
            file_name[j] = expr[i];
            j++;
        }
        file_name[j] = '\0';

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
       
        
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        rewind(file);

        char *ba = (char *)malloc(size);

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
        strcat(buffer,ba);
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
        strcat(buffer,ba);

        }
        send_expr(newsockfd, buffer, strlen(buffer));
        }
		}
		close(newsockfd);
	}
	return 0;
}
			

