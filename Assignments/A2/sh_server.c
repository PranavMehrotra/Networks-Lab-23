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
#include <dirent.h>

			/* THE SERVER PROCESS */

#define MAX_SIZE 50
#define argmax_size 2048

void remove_spaces(char *a){
	int i=0,j=0,k=strlen(a),f=1;
	if(k==0)	return;
	char b[k+1];
	while(a[i]!='\0'){
		if(a[i++]==' ' && f)	continue;
        b[j++] = a[i-1];
		if(a[i-1]==' ')    f=1;
        else f=0;
	}
    if(b[j-1]==' ') b[j-1]='\0';
	else b[j] = '\0';
	strcpy(a,b);
	// a = realloc(a,strlen(a));
}

int get_op(char *a, char *op, char *arg){
    int i=0,j=0,n=strlen(a);
    while(a[i]!='\0'){
        if(a[i]==' ' && a[i-1]!='\\')   j++;
        if(j>1) return 1;
        i++;
    }
    i=0;
    while(a[i]!='\0'){
        if(a[i]==' ' && a[i-1]!='\\')   break;
        i++;
    }
    strncpy(op,a,i);
    op[i]='\0';
    if(i<n){
        strcpy(arg,a+i+1);
    }
    else{
        strcpy(arg,".");
    }
    return 0;
}

void remove_back_slashes(char *a){
    int i=0,j=0,k=strlen(a);
	if(k==0)	return;
	char b[k+1];
	while(a[i]!='\0'){
		if(a[i++]=='\\')	continue;
        b[j++] = a[i-1];
	}
    b[j] = '\0';
	strcpy(a,b);
    // a = realloc(a,strlen(a));
}

char *recieve_expr(int newsockfd){
	char c;
	char *s;
	// printf("\n");
	int len=0,size = 50,y,chunk=10;
	s = (char *)malloc(sizeof(char)*size);
	if(!s)	return NULL;
	while(y=recv(newsockfd,s+len,chunk,0)){
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

int main()
{
	int			sockfd, newsockfd ; /* Socket descriptors */
	int			clilen;
	struct sockaddr_in	cli_addr, serv_addr;

	int i;
	
    // while(1){
    //     printf("Enter Command: ");
    //     fgets(buf,50,stdin);
    //     buf[strlen(buf)-1]= '\0';
    //     remove_spaces(buf);
    //     if(get_op(buf,op,arg)){
    //         printf("Too many arguments\n");
    //         continue;
    //     }
    //     else{
    //         printf("%s , %s\n",op,arg);
    //     }
    //     remove_back_slashes(arg);
    //     if(strcmp(op,"exit")==0)    break;
    //     else if(strcmp(op,"pwd")==0){
    //         if(getcwd(ans,argmax_size)==NULL){
    //             printf("Error in execution\n");
    //         }
    //         else{
    //             printf("\t%s\n",ans);
    //         }
    //     }
    //     else if(strcmp(op,"cd")==0){
    //         if(chdir(arg)<0){
    //             printf("Error in execution\n");
    //         }
    //         else{
    //             getcwd(ans,argmax_size);
    //             printf("\t%s\n",ans);
    //             printf("Changed successfully\n");
    //         }
    //     }
	// 	else if(strcmp(op,"dir")==0){
	// 		dir = opendir(arg);
	// 		if(dir==NULL){
	// 			printf("Error in execution\n");
	// 		}
	// 		else{
	// 			while ((comp = readdir(dir)) != NULL) 
    //         		printf("%s\n", comp->d_name); 
    // 			closedir(dir);
	// 		}
	// 	}
	// 	else{
	// 		printf("Invalid operation.\n");
	// 	}
    // }
    exit(0);

	/* The following system call opens a socket. The first parameter
	   indicates the family of the protocol to be followed. For internet
	   protocols we use AF_INET. For TCP sockets the second parameter
	   is SOCK_STREAM. The third parameter is set to 0 for user
	   applications.
	*/
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Cannot create socket\n");
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
		printf("Unable to bind local address\n");
		exit(0);
	}

	listen(sockfd, 5); /* This specifies that up to 5 concurrent client
			      requests will be queued up while the system is
			      executing the "accept" system call below.
			   */

	/* In this program we are illustrating a concurrent server -- one
	   which forks to accept multiple client connections concurrently.
	   As soon as the server accepts a connection from a client, it
	   forks a child which communicates with the client, while the
	   parent becomes free to accept a new connection. To facilitate
	   this, the accept() system call returns a new socket descriptor
	   which can be used by the child. The parent continues with the
	   original socket descriptor.
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
			printf("Accept error\n");
			exit(0);
		}

		/* Having successfully accepted a client connection, the
		   server now forks. The parent closes the new socket
		   descriptor and loops back to accept the next connection.
		*/
		if (fork() == 0) {

			/* This child process will now communicate with the
			   client through the send() and recv() system calls.
			*/
			close(sockfd);	/* Close the old socket since all
					   communications will be through
					   the new socket.
					*/

			/* We initialize the buffer, copy the message to it,
			   and send the message to the client. 
			*/
			char buf[MAX_SIZE];		/* We will use this buffer for communication */
			char op[20],arg[argmax_size],ans[argmax_size];
			struct dirent *comp;
			DIR *dir;
			strcpy(buf,"LOGIN: ");
			send(newsockfd, buf, strlen(buf) + 1, 0);

			/* We again initialize the buffer, and receive a 
			   message from the client. 
			*/
			recv(newsockfd, buf, MAX_SIZE, 0);
			printf("%s\n", buf);
            FILE *fp = fopen("users.txt","r");
            int n,f=0;
            char s[MAX_SIZE];
            while (fgets(s,MAX_SIZE,fp)){
                n = strlen(s);
                if(n<=1)    break;
                s[n-1] = '\0';
                if(strcmp(buf,s)==0){
                    f=1;
                    strcpy(buf,"FOUND");
			        send(newsockfd, buf, strlen(buf) + 1, 0);
                    break;
                }
            }
			fclose(fp);
            if(!f){
                strcpy(buf,"NOT-FOUND");
                send(newsockfd, buf, strlen(buf) + 1, 0);
                close(newsockfd);
                exit(0);
            }
            while(1){
                char *expr = recieve_expr(newsockfd);
                printf("\n");
                if(!expr || strcmp(expr,"exit")==0){
                    free(expr);
                    printf("Bye client!\n");
                    break;
                }
                remove_spaces(expr);
                if(strlen(expr)<=1){
                    free(expr);
                    continue;
                }
                if(get_op(buf,op,arg)){
					printf("Too many arguments\n");
					continue;
				}
				else{
					printf("%s , %s\n",op,arg);
				}
				remove_back_slashes(arg);
				if(strcmp(op,"exit")==0)    break;
				else if(strcmp(op,"pwd")==0){
					if(getcwd(ans,argmax_size)==NULL){
						printf("Error in execution\n");
					}
					else{
						printf("\t%s\n",ans);
					}
				}
				else if(strcmp(op,"cd")==0){
					if(chdir(arg)<0){
						printf("Error in execution\n");
					}
					else{
						getcwd(ans,argmax_size);
						printf("\t%s\n",ans);
						printf("Changed successfully\n");
					}
				}
				else if(strcmp(op,"dir")==0){
					dir = opendir(arg);
					if(dir==NULL){
						printf("Error in execution\n");
					}
					else{
						while ((comp = readdir(dir)) != NULL) 
							printf("%s\n", comp->d_name); 
						closedir(dir);
					}
				}
				else{
					printf("Invalid operation.\n");
				}
		    }
			close(newsockfd);
			exit(0);
		}

		close(newsockfd);
	}
	return 0;
}
			

