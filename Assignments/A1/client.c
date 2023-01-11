
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

int input_expr(FILE *fp, int sockfd){
	char c;
	int len = 0,size=10,f=0,y;
	char s[size+1];
	s[size]=  '\0';
	while((c=fgetc(fp))!=EOF && c!='\n'){
		s[len++] = c;
		if(len==size){
			y = send(sockfd, s, size, 0);
			if(y!=size){
				printf("\nError: Failed to send data to the server. Please try again.\n");
				return 0;
			}
			len=0;
			f=1;
			// printf("One chunk sent. '%s'\n",s);
		}
	}
	s[len++] = '\0';
	if(f==0 && strcmp(s,"-1")==0){
		char buf[10] = "close";
		send(sockfd, buf, strlen(buf) + 1, 0);
		printf("Bye!\n");
		return 1;
	}
	y=send(sockfd, s, len, 0);
	if(y!=len){
		printf("\nError: Failed to send data to the server. Please try again.\n");
		return 0;
	}
	// printf("Last chunk sent. '%s'\n",s);
	return 0;
}



int main()
{
	int			sockfd ;
	struct sockaddr_in	serv_addr;

	int i;

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
		char *expr;
		if(input_expr(stdin,sockfd))	break;
		float ans;
		int x = recv(sockfd, &ans, sizeof(float), 0);
		if(x!=sizeof(float)){
			printf("Cannot fetch results from the server. Please try again later.\n");
			break;
		}
		printf("Value of the expression: %.4f\n",ans);
	}
	close(sockfd);
	return 0;

}

