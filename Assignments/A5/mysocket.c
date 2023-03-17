#include "mysocket.h"

//gloabl send queue
queue_head *send_queue;

//global recieve queue
queue_head *receive_queue;

int my_type=0;          //if 1 then socket is of type My_TCP
int curr_sockfd=-1;     //current socket fd will be stored and passed to different functions

//thread declaration
pthread_t R_thread;
pthread_t S_thread;

//thread's attribute declaration
pthread_attr_t R_attr;
pthread_attr_t S_attr;

// Pthread mutex locks
pthread_mutex_t send_mutex;
pthread_mutex_t receive_mutex;

int my_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    //wrapper around bind call
    return bind(sockfd, addr, addrlen);
}

int my_listen(int sockfd, int backlog)
{
    //wrapper around listen call
    return listen(sockfd, backlog);
}

int my_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    //current socket fd updated and connect called
    curr_sockfd = sockfd;
    return connect(sockfd,addr,addrlen);
}

int my_accept(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen)
{
    //current socket fd updated and accept called
    return curr_sockfd = accept(sockfd, addr, addrlen);
}


// Helper functions
//the function will recieve 4 bytes i.e. one integer using recv call
int recv_num(int newsockfd){
    char num[10];
    int ans;
    int y,len=0,siz=sizeof(int);
    //till 4 bytes are recieved, keep reciveing in chunks
    while((siz>len) && ((y=recv(newsockfd,num+len,siz-len,0))>0)){
        len+=y;
        if(len==siz){
            memcpy(&ans,num,siz);
            // printf("Recieved size = %d\n",ans);
            break;
        }
    }
    return ans;
}

int min(int a,int b){
    //returns the minimum of a and b
    return (a<b)?a:b;
}

//receive an expression/string in chunks
char *recieve_expr(int newsockfd, int *len_recieved){
	char *s;

    //the first 4 bytes of all messages sent using my_send would be reserved for the length of message.
    //store the length of the message in recv_size
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
	
    //send the rest of message i.e. expr in chunks
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
//function returns 0 on succesful push and 1 in case of failure.
int push(queue_head *head, char *s, int len){

    //if head is null
    if(!head)   return 1;

    //if the queue is full
    if(head->curr_size == head->max_size)  return 1;

    //create a new node and update pointers
    queue *new_node = (queue *)malloc(sizeof(queue));
    new_node->data = s;
    new_node->len = len;
    new_node->next = NULL;
    
    //the new node is added to the rear
    head->rear->next = new_node;
    head->rear = new_node;
    head->curr_size++;
    return 0;
}

// Pop from the queue
//the function returns the message on successful pop and NULL in case of failure
char *pop(queue_head *head, int *len){

    //if head is null
    if(!head)   return NULL;

    //if the queue is empty
    if(head->curr_size == 0)  return NULL;

    //the node from front is popped and pointers are updated
    queue *temp = head->front->next;
    char *s = temp->data;
    *len = temp->len;
    head->front->next = temp->next;

    if(head->rear == temp)  head->rear = head->front;
    head->curr_size--;
    free(temp);
    return s;
}

// Cleanup function for R
void cleanup_R(void *arg){
    // printf("Cleaning R\n");
    
    // Free the Receive queue
    queue *temp = receive_queue->front->next;
    // int t=1;
    while(temp){
        // printf("R Cleaning %d\n",t++);
        queue *temp1 = temp->next;
        free(temp->data);
        free(temp);
        temp = temp1;
    }
    free(receive_queue->front);
    free(receive_queue);
    
    // Destroy mutex lock
    pthread_mutex_destroy(&receive_mutex);

}

// Cleanup function for S
void cleanup_S(void *arg){
    // printf("Cleaning S\n");
    
    // Free the Send queue
    queue *temp = send_queue->front->next;
    // int t=1;
    while(temp){
        // printf("S Cleaning %d\n",t++);
        queue *temp1 = temp->next;
        free(temp->data);
        free(temp);
        temp = temp1;
    }
    free(send_queue->front);
    free(send_queue);
    
    // Destroy mutex lock
    pthread_mutex_destroy(&send_mutex);
}

// R Thread to wait on recv() call and put the message in the receive table
void *R(void *arg)
{
    // Register the cleanup function
    pthread_cleanup_push(cleanup_R, NULL);
    
    // Poll Structure 
    struct pollfd fdset[1];
    
    //check for incoming message
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
        //recieve teh incoming message
        int len=0,ret=1;
        char *s = recieve_expr(temp_sockfd, &len);
        if(len==0)  continue;
        
        //try pushing the message to the table, else sleep and repeat
        while(ret){

            // Gather receive_queue lock
            pthread_mutex_lock(&receive_mutex);
            //put in receive table
            ret = push(receive_queue, s, len);
            // Release receive_queue lock
            pthread_mutex_unlock(&receive_mutex);
            if(ret) usleep(SLEEP_TIME);
        }
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
        usleep(SLEEP_TIME);
    
        // Test Cancel
        pthread_testcancel();
        if(curr_sockfd==-1) continue;
        //wake
        while(1){
            //see if message pending
            int len=0;
            // Gather send_queue lock
            pthread_mutex_lock(&send_mutex);
            char *s = pop(send_queue, &len);
            // Release send_queue lock
            pthread_mutex_unlock(&send_mutex);
            if(len==0 || s==NULL)  break;
            //send in one or more call
            send_expr(curr_sockfd, s, len);
            free(s);
        }
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

    //Initialise send and recieve table
    send_queue = (queue_head *)malloc(sizeof(queue_head));
    send_queue->front = (queue *)malloc(sizeof(queue));
    send_queue->front->next = NULL;
    send_queue->rear = send_queue->front;
    send_queue->curr_size = 0;
    send_queue->max_size = MAX_QUEUE_SIZE;
    receive_queue = (queue_head *)malloc(sizeof(queue_head));
    receive_queue->front = (queue *)malloc(sizeof(queue));
    receive_queue->front->next = NULL;
    receive_queue->rear = receive_queue->front;
    receive_queue->curr_size = 0;
    receive_queue->max_size = MAX_QUEUE_SIZE;

    // Intialise the mutex locks
    if(pthread_mutex_init(&send_mutex, NULL) < 0){
        perror("userSimulator::pthread_mutex_init() failed");
        exit(0);
    }
    if(pthread_mutex_init(&receive_mutex, NULL) < 0){
        perror("userSimulator::pthread_mutex_init() failed");
        exit(0);
    }


    //create R and S
    if(pthread_attr_init(&R_attr) < 0){
        perror("userSimulator::pthread_attr_init() failed");
        exit(0);
    }
    pthread_attr_setdetachstate(&R_attr, PTHREAD_CREATE_DETACHED);
    if(pthread_create(&R_thread , &R_attr , R , NULL) < 0){
        perror("userSimulator::pthread_create() failed");
        exit(0);
    }
    if(pthread_attr_init(&S_attr) < 0){
        perror("userSimulator::pthread_attr_init() failed");
        exit(0);
    }
    pthread_attr_setdetachstate(&S_attr, PTHREAD_CREATE_DETACHED);
    if(pthread_create(&S_thread , &S_attr , S , NULL) < 0){
        perror("userSimulator::pthread_create() failed");
        exit(0);
    }

    return sockfd;
}

int my_close(int sockfd)
{
    if(my_type){
        sleep(MY_CLOSE_SLEEP);
        my_type = 0;
        curr_sockfd = -1;
        //kill R and S threads

        //pthread_cancel will call corresponding cleanup function before killing
        pthread_cancel(R_thread);
        pthread_cancel(S_thread);
        // printf("R and S threads killed\n");
    }
    return close(sockfd);
}

ssize_t my_send(int sockfd, const void *buf, size_t len, int flags){
    if(my_type){
        //put in send table
        if(len<=0) return 0;
        char *s = (char *)malloc(sizeof(char)*len);
        memcpy(s, buf, len);
        int ret=1;
        while(ret){
            // Gather send_queue lock
            pthread_mutex_lock(&send_mutex);
            ret = push(send_queue, s, len);
            // Release send_queue lock
            pthread_mutex_unlock(&send_mutex);
            if(ret) usleep(SLEEP_TIME);
        }
        return len;
    }
    else return send(sockfd, buf, len, flags);
}
ssize_t my_recv(int sockfd, void *buf, size_t len, int flags){
    if(my_type){
        while(1){
            //put in receive table
            int siz=0;
            // Gather receive_queue lock
            pthread_mutex_lock(&receive_mutex);
            char *s = pop(receive_queue, &siz);
            // Release receive_queue lock
            pthread_mutex_unlock(&receive_mutex);
            if(siz==0 || s==NULL){
                // printf("sleeping for %d microseconds\n", SLEEP_TIME);
                usleep(SLEEP_TIME);
                // printf("Done Sleeping\n");
                continue;
            }
            memcpy(buf, s, min(siz,len));
            free(s);
            return min(siz,len);
        }
    }
    else return recv(sockfd, buf, len, flags);
}