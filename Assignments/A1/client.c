
/*    THE CLIENT PROCESS */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define MAX_SIZE 200

char * input_expr(FILE *fp){
	char c;
	char *s;
	int len = 0,size=100;
	s = (char *)malloc(sizeof(char)*size);
	if(!s)	return NULL;
	while((c=fgetc(fp))!=EOF && c!='\n'){
		s[len++] = c;
		if(len==size){
			s = realloc(s,sizeof(char)*(size+=100));
			if(!s)	return NULL;
		}
	}
	s[len++] = '\0';

	s = realloc(s,sizeof(char)*len);
	return s;
}



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
	while(1){
		printf("Enter a valid expression(Enter \"-1\" to terminate): ");
		// scanf("%[^/n]%*c",buf);
		char *expr = input_expr(stdin);
		printf("%s\n\n",expr);
		int y;
		if(strcmp(expr,"-1")==0){
			strcpy(buf,"close");
			send(sockfd, buf, strlen(buf) + 1, 0);
			free(expr);
			printf("Bye!\n\n");
			break;
		}
		else
			y=send(sockfd, expr, strlen(expr) + 1, 0);
		free(expr);
		printf("\t%d\n",y);
		int x = recv(sockfd, buf, MAX_SIZE, 0);
		printf("Value of the expression: %7s\n\t%d\n",buf,x);
	}
	close(sockfd);
	return 0;

}

