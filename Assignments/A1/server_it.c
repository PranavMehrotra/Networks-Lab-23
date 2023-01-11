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


// Helper functions

int isNum(char ch)
{
    return ((ch >= '0' && ch <= '9')||ch=='.');
}

void remove_spaces(char *a){
	int i=0,j=0,k=strlen(a);
	if(k==0)	return;
	char b[k+1];
	while(a[i]!='\0'){
		if(a[i++]==' ')	continue;
		b[j++] = a[i-1];
	}
	b[j] = '\0';
	strcpy(a,b);
	a = realloc(a,strlen(a));
}

float calc_val(char *, int , int );

float get_num(char *a, int *i, int j){
	if(*i>=j)	return 0;
	if(a[*i]=='('){
		int p = *i+1;
		int t=1;
		while(t>0 && p<j){
			if(a[p]=='(')	t++;
			else if(a[p]==')')	t--;
			p++;
		}
		if(t)	return __INT_MAX__;
		float b = calc_val(a,*i+1,p-1);
		*i = p;
		return b;
	}
	float b;
	char c[j-*i+1];
	int p=*i;
	while(*i<j && isNum(a[*i]))	(*i)++;
	// printf("\n%s,  %d  ,  %d",a,p,*i);
	// printf("\n\t\t%s\n",c);
	strncpy(c,a+p,*i-p);
	c[*i-p]='\0';
	b = atof(c);
	// printf("\n\t\t%s\n",c);
	return b;
}

float expr_val(float a, char op, float b){
	if(op=='+')	return a+b;
	else if(op=='-')	return a-b;
	else if(op=='*')	return a*b;
	else if(op=='/')	return a/b;
	else return 0;
}


float calc_val(char *arr, int i, int j){
	float a = get_num(arr,&i,j);
	if(a==__INT_MAX__)	return 0;
	float b;
	while (i<j){
		printf("%f\t",a);
		char op = arr[i++];
		printf("%c\t",op);
		b = get_num(arr,&i,j); 
		printf("%f\t = ",b);
		if(b==__INT_MAX__)	return 0;
		a = expr_val(a,op,b);
		printf("%f\n",a);
	}
	return a;
}


char *recieve_expr(int newsockfd){
	char c;
	char *s;
	printf("\n");
	int len=0,size = 50,y,chunk=10;
	s = (char *)malloc(sizeof(char)*size);
	if(!s)	return NULL;
	while(y=recv(newsockfd,s+len,chunk,0)){
		len+=y;
		if(s[len-1]=='\0')	break;
		while(len+chunk>=size){
			s = realloc(s,sizeof(char)*(size*=2));
			if(!s)	return NULL;
			printf("\t%d",size);
		}
	}
	if(len==0){
		free(s);
		return NULL;
	}
	s = realloc(s,sizeof(char)*len);
	return s;
}


		/* THE SERVER PROCESS */

int main()
{
	int			sockfd, newsockfd ; /* Socket descriptors */
	int			clilen;
	struct sockaddr_in	cli_addr, serv_addr;

	int i;

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
	serv_addr.sin_port		= htons(20000);

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
		// send(newsockfd, buf, strlen(buf) + 1, 0);

		/* We now receive a message from the client. For this example
  		   we make an assumption that the entire message sent from the
  		   client will come together. In general, this need not be true
		   for TCP sockets (unlike UDPi sockets), and this program may not
		   always work (for this example, the chance is very low as the 
		   message is very short. But in general, there has to be some
	   	   mechanism for the receiving side to know when the entire message
		  is received. Look up the return value of recv() to see how you
		  can do this.
		*/ 
		while(1){
			char *expr = recieve_expr(newsockfd);
			printf("\n");
			if(!expr || strcmp(expr,"close")==0){
				free(expr);
				printf("Bye client!\n");
				break;
			}	
			// printf("'%s'\n%d",expr,strlen(expr));
			remove_spaces(expr);
			// printf("'%s'\n%d",expr,strlen(expr));
			float ans = calc_val(expr,0,strlen(expr));
			free(expr);
			printf("%f\n",ans);
			// gcvt(ans,20,buf);
			send(newsockfd, &ans, sizeof(float), 0);
		}
		close(newsockfd);
	}
	close(sockfd);

	return 0;
}
			

