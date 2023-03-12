#include "mysocket.h"

// Global variables of queue
queue_head *send_queue;
queue_head *receive_queue;
int my_type=0;
int curr_sockfd=-1;
pthread_t R_thread;
pthread_t S_thread;
pthread_attr_t R_attr;
pthread_attr_t S_attr;

int my_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    return bind(sockfd, addr, addrlen);
}

int my_listen(int sockfd, int backlog)
{
    return listen(sockfd, backlog);
}

int my_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    curr_sockfd = sockfd;
    return connect(sockfd,addr,addrlen);
}

int my_accept(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen)
{
    return curr_sockfd = accept(sockfd, addr, addrlen);
}


// Helper functions
int recv_num(int newsockfd){
    char num[10];
    int ans;
    int y,len=0,siz=sizeof(int);
    while((siz>len) && ((y=recv(newsockfd,num+len,siz-len,0))>0)){
        len+=y;
        if(len==siz){
            memcpy(&ans,num,siz);
            printf("Recieved size = %d\n",ans);
            break;
        }
    }
    return ans;
}

int min(int a,int b){
    return (a<b)?a:b;
}

//receive an expression/string in chunks
char *recieve_expr(int newsockfd, int *len_recieved){
	char *s;
	int len=0,size = 2*MAX_CHUNK_SIZE,y,chunk=MAX_CHUNK_SIZE,recv_size = recv_num(newsockfd);
    s = (char *)malloc(sizeof(char)*size);
	if(!s)	return NULL;
    //keep on receiving characters in chunks and store them in buffer
	while((recv_size > len) && ((y=recv(newsockfd,s+len,min(recv_size-len,chunk),0))>0)){
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
        *len_recieved = 0;
		return NULL;
	}
	s = realloc(s,sizeof(char)*(len+1));
    *len_recieved = len;
    return s;
    //return the message read and update the length
}

//send expression in chunks
void send_expr(int newsockfd, char *expr, int size){
	int len=0,y,chunk=MAX_CHUNK_SIZE;
    // Send the size of the message
    send(newsockfd,&size,sizeof(int),0);
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


// Push to the queue
void push(queue_head *head, char *s, int len){
    queue *new_node = (queue *)malloc(sizeof(queue));
    new_node->data = s;
    new_node->len = len;
    new_node->next = NULL;
    
    head->rear->next = new_node;
    head->rear = new_node;
}

// Pop from the queue
char *pop(queue_head *head, int *len){
    if(head->front == head->rear)  return NULL;
    queue *temp = head->front->next;
    char *s = temp->data;
    *len = temp->len;
    head->front->next = temp->next;
    if(head->rear == temp)  head->rear = head->front;
    free(temp);
    return s;
}

// Cleanup function for R
void cleanup_R(void *arg){
    printf("Cleaning R\n");
    // Free the Receive queue
    queue *temp = receive_queue->front->next;
    while(temp){
        queue *temp1 = temp->next;
        free(temp->data);
        free(temp);
        temp = temp1;
    }
    free(receive_queue->front);
    free(receive_queue);
}

// Cleanup function for S
void cleanup_S(void *arg){
    printf("Cleaning S\n");
    // Free the Send queue
    queue *temp = send_queue->front->next;
    while(temp){
        queue *temp1 = temp->next;
        free(temp->data);
        free(temp);
        temp = temp1;
    }
    free(send_queue->front);
    free(send_queue);
}

// R Thread to wait on recv() call and put the message in the receive table
void *R(void *arg)
{
    // Register the cleanup function
    pthread_cleanup_push(cleanup_R, NULL);
    // Poll Structure
    struct pollfd fdset[1];
    int timeout=POLL_TIMEOUT,ret,temp_sockfd;
    while(1){
        temp_sockfd = curr_sockfd;
        if(temp_sockfd==-1){
            // Test Cancel
            pthread_testcancel();
            continue;
        }
        fdset[0].fd = temp_sockfd;
        fdset[0].events = POLLIN;
        // Poll for message with timeout 1 sec
        ret = poll(fdset,1,timeout);
		if(ret<0){
			perror("Poll error. Please restart the load balancer.\n");
			cleanup_R(NULL);
            pthread_exit(0);
		}
        if(ret==0){
            // Timeout
            // Test Cancel
            pthread_testcancel();
            continue;
        }
        int len=0;
        char *s = recieve_expr(temp_sockfd, &len);
        if(len==0)  continue;
        //put in receive table
        push(receive_queue, s, len);
    }
    pthread_cleanup_pop(1);
    pthread_exit(0);
}

//s sleep for t time
//wake
//see if message pending
//send in one or more call
void *S(void *arg)
{
    // Register the cleanup function
    pthread_cleanup_push(cleanup_S, NULL);
    while(1){
        //sleep for t time
        sleep(SLEEP_TIME);
        // Test Cancel
        pthread_testcancel();
        //wake
        //see if message pending
        int len=0;
        char *s = pop(send_queue, &len);
        if(len==0 || s==NULL)  continue;
        //send in one or more call
        send_expr(curr_sockfd, s, len);
        free(s);
    }
    pthread_cleanup_pop(1);
    pthread_exit(0);
}


int my_socket(int domain, int type, int protocol)
{
    int sockfd;
    if(type == SOCK_MyTCP)
    {
        my_type = 1;
        sockfd  = socket(AF_INET, SOCK_STREAM, 0);
    }
    else return socket(domain, type, protocol);

    //Initialise send recieve table
    //create R and S
    if(pthread_attr_init(&R_attr) < 0){
        exit_with_error("userSimulator::pthread_attr_init() failed");
    }
    if(pthread_create(&R_thread , &R_attr , R , NULL) < 0){
        exit_with_error("userSimulator::pthread_create() failed");
    }
    if(pthread_attr_init(&S_attr) < 0){
        exit_with_error("userSimulator::pthread_attr_init() failed");
    }
    if(pthread_create(&S_thread , &S_attr , S , NULL) < 0){
        exit_with_error("userSimulator::pthread_create() failed");
    }

    return sockfd;
}

int my_close(int sockfd)
{
    if(my_type){
        if(curr_sockfd == sockfd){
            curr_sockfd = -1;
        }
        else{
            my_type = 0;
            curr_sockfd = -1;
            //kill R and S threads
            pthread_cancel(R_thread);
            pthread_cancel(S_thread);
            
        }
    }
    return close(sockfd);
}

ssize_t my_send(int sockfd, const void *buf, size_t len, int flags){
    if(my_type){
        //put in send table
        if(len<=0) return 0;
        char *s = (char *)malloc(sizeof(char)*len);
        memcpy(s, buf, len);
        push(send_queue, s, len);
        return len;
    }
    else return send(sockfd, buf, len, flags);
}
ssize_t my_recv(int sockfd, void *buf, size_t len, int flags){
    if(my_type){
        //put in receive table
        int len=0;
        char *s = pop(receive_queue, &len);
        if(len==0 || s==NULL)  return 0;
        memcpy(buf, s, len);
        free(s);
        return len;
    }
    else return recv(sockfd, buf, len, flags);
}