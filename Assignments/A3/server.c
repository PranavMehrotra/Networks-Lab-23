/*
			NETWORK PROGRAMMING WITH SOCKETS

In this program we illustrate the use of Berkeley sockets for interprocess
communication across the network. We show the communication between a server
process and a client process.


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
#define MAX_SIZE 50

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

void send_expr(int newsockfd, char *expr){
	int len=0,size = strlen(expr),y,chunk=MAX_SIZE;
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


			/* THE SERVER PROCESS */

int main(int argc, char** argv)
{	
	if(argc!=2){
		printf("Usage: ./server <port_number>\n");
		exit(0);
	}
	int port_num = atoi(argv[1]);
	int			sockfd, newsockfd ; /* Socket descriptors */
	int			clilen;
	struct sockaddr_in	cli_addr, serv_addr;

	int i;
	char buf[MAX_SIZE];		/* We will use this buffer for communication */
	time_t rawtime;
	struct tm * timeinfo;
	srand(time(0));

	/* The following system call opens a socket. The first parameter
	   indicates the family of the protocol to be followed. For internet
	   protocols we use AF_INET. For TCP sockets the second parameter
	   is SOCK_STREAM. The third parameter is set to 0 for user
	   applications.
	*/
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Cannot create socket\n");
		exit(0);
	}

	/* The structure "sockaddr_in" is defined in <netinet/in.h> for the
	   internet family of protocols. This has three main fields. The
 	   field "sin_family" specifies the family and is therefore AF_INET
	   for the internet family. The field "sin_addr" specifies the
	   internet address of the server. This field is set to INADDR_ANY
	   for machines having a single IP address. The field "sin_port"
	   specifies the port number of the server.
	*/

	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= INADDR_ANY;
	serv_addr.sin_port		= htons(port_num);

	/* With the information provided in serv_addr, we associate the server
	   with its port using the bind() system call. 
	*/
	if (bind(sockfd, (struct sockaddr *) &serv_addr,
					sizeof(serv_addr)) < 0) {
		perror("Unable to bind local address\n");
		exit(0);
	}

	listen(sockfd, 5); /* This specifies that up to 5 concurrent client
			      requests will be queued up while the system is
			      executing the "accept" system call below.
			   */

	/* In this program we are illustrating an iterative server -- one
	   which handles client connections one by one.i.e., no concurrency.
	   The accept() system call returns a new socket descriptor
	   which is used for communication with the server. After the
	   communication is over, the process comes back to wait again on
	   the original socket descriptor.
	*/
	

	while (1) {

		/* The accept() system call accepts a client connection.
		   It blocks the server until a client request comes.

		   The accept() system call fills up the client's details
		   in a struct sockaddr which is passed as a parameter.
		   The length of the structure is noted in clilen. Note
		   that the new socket descriptor returned by the accept()
		   system call is stored in "newsockfd".
		*/
		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr,
					&clilen) ;

		if (newsockfd < 0) {
			perror("Accept error\n");
			exit(0);
		}


		/* We initialize the buffer, copy the message to it,
			and send the message to the client. 
		*/
		char *expr = recieve_expr(newsockfd);
		if(!expr)	continue;
		if(strcmp(expr,"Send Load")==0){
			int load = ((rand()%100)+1);
			send(newsockfd, &load, sizeof(int), 0);
			printf("Load sent: %d\n",load);
		}
		else{
			time ( &rawtime );
			timeinfo = localtime ( &rawtime );
			strcpy(buf, asctime(timeinfo));
			send_expr(newsockfd, buf);
		}

		close(newsockfd);
	}

	return 0;
}
			

