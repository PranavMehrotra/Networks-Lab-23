
/*    THE CLIENT PROCESS */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define MAX_SIZE 50

int input_expr(FILE *fp, int sockfd){
	char c;
	int len = 0,size=MAX_SIZE,f=0,y;
	char s[size];
	// char s[size+1];
	// s[size]=  '\0';
	while((c=fgetc(fp))!=EOF && c!='\n'){
		s[len++] = c;
		if(len==size){
			y = send(sockfd, s, size, 0);
			if(y!=size){
				printf("\nError: Failed to send data to the server. Please try again later.\n");
				return 1;
			}
			len=0;
			f=1;
			// printf("One chunk sent. '%s'\n",s);
		}
	}
	s[len++] = '\0';
	if(f==0 && strcmp(s,"exit")==0){
		char buf[10] = "exit";
		send(sockfd, buf, strlen(buf) + 1, 0);
		printf("Bye!\n");
		return 1;
	}
	// printf("%s\n",s);
	y=send(sockfd, s, len, 0);
	if(y!=len){
		printf("\nError: Failed to send data to the server. Please try again later.\n");
		return 1;
	}
	// printf("Last chunk sent. '%s'\n",s);
	return 0;
}

char *recieve_expr(int newsockfd){
	char c;
	char *s;
	// printf("\n");
	int len=0,size = MAX_SIZE,y,chunk=10;
	s = (char *)malloc(sizeof(char)*size);
	if(!s)	return NULL;
	// printf("Hello\n");
	while((y=recv(newsockfd,s+len,chunk,0))>0){
		len+=y;
		if(s[len-1]=='\0')	break;
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
	s = realloc(s,sizeof(char)*len);
	return s;
}

// void send_expr(int newsockfd, char *expr){
// 	int len=0,size = strlen(expr),y,chunk=MAX_SIZE;
// 	if(size<chunk){
// 		send(newsockfd,expr,size+1,0);
// 		return;
// 	}
// 	while(1){
// 		if(len+chunk>size){
// 			send(newsockfd,expr+len,size-len+1,0);
// 			break;
// 		}
// 		y=send(newsockfd,expr+len,chunk,0);
// 		len+=y;
// 	}
// }


int main()
{
	int			sockfd ;
	struct sockaddr_in	serv_addr;

	int i;
    char buf[MAX_SIZE];
	/* Opening a socket is exactly similar to the server process */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Unable to create socket\n");
		exit(0);
	}

	/* Recall that we specified INADDR_ANY when we specified the server
	   address in the server. Since the client can run on a different
	   machine, we must specify the IP address of the server. 

	   In this program, we assume that the server is running on the
	   same machine as the client. 127.0.0.1 is a special address
	   for "localhost" (this machine)
	   
	/* IF YOUR SERVER RUNS ON SOME OTHER MACHINE, YOU MUST CHANGE 
           THE IP ADDRESS SPECIFIED BELOW TO THE IP ADDRESS OF THE 
           MACHINE WHERE YOU ARE RUNNING THE SERVER. 
    	*/

	serv_addr.sin_family	= AF_INET;
	inet_aton("127.0.0.1", &serv_addr.sin_addr);
	serv_addr.sin_port	= htons(20000);

	/* With the information specified in serv_addr, the connect()
	   system call establishes a connection with the server process.
	*/
	if ((connect(sockfd, (struct sockaddr *) &serv_addr,
						sizeof(serv_addr))) < 0) {
		perror("Unable to connect to server\n");
		exit(0);
	}

	/* After connection, the client can send or receive messages.
	   However, please note that recv() will block when the
	   server is not sending and vice versa. Similarly send() will
	   block when the server is not receiving and vice versa. For
	   non-blocking modes, refer to the online man pages.
	*/
	char *expr1,*expr2;
    expr1 = recieve_expr(sockfd);
    printf("%s", expr1);
    // scanf("%s", buf);
    // printf("%s\n", buf);
	free(expr1);
    input_expr(stdin,sockfd);
	printf("Username sent\n");
    expr2 = recieve_expr(sockfd);
	printf("Username sent %s\n",expr2);
    if(strcmp(expr2, "NOT-FOUND")==0){
        printf("Invalid Username\n");
        close(sockfd);
		free(expr2);
        exit(0);
    }
    // printf("%s\n", buf);
    // fgets(buf, MAX_SIZE, stdin);
	free(expr2);
	while(1){
        printf("Enter a shell command: ");
		char *expr;
        if(input_expr(stdin,sockfd))	break;
		expr = recieve_expr(sockfd);
        if(strcmp(expr,"$$$$")==0){
            printf("Invalid command\n");
        }
        else if(strcmp(expr,"####")==0){
            printf("Error in running command\n");
        }
        else{
            printf("%s\n",expr);
        }
        free(expr);
	}
	close(sockfd);
	return 0;

}

