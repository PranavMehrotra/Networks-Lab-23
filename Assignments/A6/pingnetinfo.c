#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <netdb.h>
#include <ifaddrs.h>

#define PACKET_SIZE 28
#define MAX_RECV_SIZE 128
#define ICMP_HDR_SIZE 8
#define IP_HDR_SIZE 20
#define START_ASCII 33
#define END_ASCII 126
#define NUM_PING_TRY 5
#define TIME_IN_MICRO 1
#define TIME_IN_MILLI 0
#define ROUTE_PROBE_TIME 2000000
#define LATENCY_NA 1e12
#define DATA_PACKET_SIZE 1008
#define DATA_RECV_SIZE 1096

const int MAX_HOPS = 30;

char out_file[200];
FILE *fp;

unsigned short in_cksum(unsigned short *addr, int len) {
    unsigned int sum = 0;
    while (len > 1) {
        sum += *addr;
        addr++;
        len -= 2;
    }
    if (len == 1) {
        sum += *(unsigned char *)addr;
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

void print_ip_header(unsigned char *buf){
    struct iphdr *ip_hdr = (struct iphdr *)buf;
    fprintf(fp, "IP header:\n");
    fprintf(fp, "  Checksum: %d\n", ip_hdr->check);
    fprintf(fp, "  Version: %d\n", ip_hdr->version);
    fprintf(fp, "  Source: %s\n", inet_ntoa(*(struct in_addr *)&ip_hdr->saddr));
    fprintf(fp, "  Destination: %s\n", inet_ntoa(*(struct in_addr *)&ip_hdr->daddr));
    fprintf(fp, "  Header length: %d\n", ip_hdr->ihl);
    fprintf(fp, "  Total length: %d\n", ntohs(ip_hdr->tot_len));
    fprintf(fp, "  TTL: %d\n", ip_hdr->ttl);
    fprintf(fp, "  ID: %d\n", ntohs(ip_hdr->id));
    if(ip_hdr->protocol == IPPROTO_TCP)
        fprintf(fp, "  Protocol: TCP\n");
    else if(ip_hdr->protocol == IPPROTO_UDP)
        fprintf(fp, "  Protocol: UDP\n");
    else if(ip_hdr->protocol == IPPROTO_ICMP)
        fprintf(fp, "  Protocol: ICMP\n");
    else
        fprintf(fp, "  Protocol: Unknown Protocol\n");
    fprintf(fp, "  Fragment offset: %d\n", ip_hdr->frag_off);
    fflush(fp);
}

void print_icmp_header(unsigned char *buf){
    struct icmphdr *icmp_hdr = (struct icmphdr *)(buf);
    fprintf(fp, "ICMP header:\n");
    fprintf(fp, "  Type: %d\n", icmp_hdr->type);
    fprintf(fp, "  Code: %d\n", icmp_hdr->code);
    fprintf(fp, "  ID: %d\n", ntohs(icmp_hdr->un.echo.id));
    fprintf(fp, "  Sequence: %d\n", ntohs(icmp_hdr->un.echo.sequence));
    fprintf(fp, "  Checksum: %d\n", icmp_hdr->checksum);
    fprintf(fp, "\n------------------------------------------\n\n");
    fflush(fp);
}

void print_payload(unsigned char *buf, int len) {
    int i;
    fprintf(fp, "Payload:\n");
    fprintf(fp, "Length: %d bytes\n", len);
    for (i = 0; i < len; i++) {
        fprintf(fp, "%02x ", buf[i]);
        if(i%8==7) printf("\n");
    }
    fprintf(fp, "\n\n");
}

// Function to get difference between two timespecs in micro or milli seconds, as specified by the flag
double get_time_diff(struct timespec *start, struct timespec *end, int flag) {
    double diff = (end->tv_sec - start->tv_sec) * 1000000 + ((double)end->tv_nsec - start->tv_nsec) / 1000.0;
    if (flag == TIME_IN_MILLI) {
        diff /= 1000.0;
    }
    return diff;
}

// Function to return the IP address of the machine
char *get_ip_addr() {
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];
    
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }
    
    // Walk through linked list, maintaining head pointer so we can free list later
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }
        
        family = ifa->ifa_addr->sa_family;
        
        // For an AF_INET* interface address, display the address
        if (family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                exit(EXIT_FAILURE);
            }
            
            if (strcmp(ifa->ifa_name, "lo") != 0) {
                freeifaddrs(ifaddr);
                return strdup(host);
            }
        }
    }
    
    freeifaddrs(ifaddr);
    return NULL;
}

// Function to return the minimum of two numbers
double min(double a, double b) {
    return ((a < b)? a : b);
}

// Function to return the maximum of two numbers
double max(double a, double b) {
    return ((a > b)? a : b);
}

// Function to add sequential binary numbers to the ICMP payload, given the starting number and the number of bytes to add
void add_seq_chars(unsigned char *buf, int num_bytes) {
    int i,j;
    int start = START_ASCII;
    int size_of_one = END_ASCII - START_ASCII + 1;
    for(j=0;j<(num_bytes/size_of_one);j++){
        for (i = 0; i < size_of_one; i++) {
            buf[j*size_of_one+i] = start + i;
        }
    }
    for(i=0;i<(num_bytes%size_of_one);i++){
        buf[j*size_of_one+i] = start + i;
    }
}

void get_next_hop(int sockfd, char* packet,struct sockaddr_in* dest_addr,struct in_addr* dest_ip, char *next_hop, int time_unit){
    char recv_buf[MAX_RECV_SIZE];
    struct sockaddr_in recv_addr;
    socklen_t recv_addr_len = sizeof(recv_addr);
    char next_router[INET_ADDRSTRLEN];
    int recv_len;
    printf("Sending %d ICMP packets each %d seconds apart to finalise the next hop\n",NUM_PING_TRY,(ROUTE_PROBE_TIME/1000000));
    struct timespec start, end;
    int sleep_time_in_micro = ROUTE_PROBE_TIME;
    double diff=0;
    memset(next_hop, 0, INET_ADDRSTRLEN);
    printf("RTT: \t");
    for(int i=0;i<NUM_PING_TRY;i++){
        clock_gettime(CLOCK_MONOTONIC, &start);
        // Send the ICMP packet to the destination
        if (sendto(sockfd, packet, PACKET_SIZE, 0, (struct sockaddr *)dest_addr, sizeof(*dest_addr)) <= 0) {
            printf("Error sending packet: %s\n", strerror(errno));
            exit(0);
        }
        
        // Receive the response from the router
        if ((recv_len = recvfrom(sockfd, recv_buf, MAX_RECV_SIZE, 0, (struct sockaddr *)&recv_addr, &recv_addr_len)) <= 0) {
            printf("*\t");
            fflush(stdout);
            strcpy(next_hop,"0.0.0.0");
            continue;
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        diff = get_time_diff(&start, &end, time_unit);
        // Print the IP address of the router that responded
        if (inet_ntop(AF_INET, &recv_addr.sin_addr, next_router, INET_ADDRSTRLEN) == NULL) {
            printf("Invalid Source IP address\n");
            continue;
        }
        if(time_unit == TIME_IN_MICRO)
            printf("%.3lf us\t", diff);
        else
            printf("%.3lf ms\t", diff);
        fflush(stdout);
        strcpy(next_hop,next_router);
        
        print_ip_header(recv_buf);
        print_icmp_header(recv_buf + IP_HDR_SIZE);

        if(((long)diff)<sleep_time_in_micro && i!=NUM_PING_TRY-1){
            // printf("Sleeping for %ld microseconds\n",sleep_time_in_micro-(long)diff);
            usleep(sleep_time_in_micro-(long)diff);
        }
    }
    printf("\nNext hop is %s\n",next_hop);
    fflush(stdout);
}

// Function to calculate latency of the path
void calc_latency(int sockfd, char* packet,struct sockaddr_in* next_addr, int ttl, double *latencies, int n, long sleep_time_in_micro, int time_unit){
    struct timespec start, end;
    char recv_buf[MAX_RECV_SIZE];
    struct sockaddr_in recv_addr;
    socklen_t recv_addr_len = sizeof(recv_addr);
    int recv_len;
    double time_taken=0,min_time_taken=LATENCY_NA;
    printf("RTT: \t");
    for(int i=0;i<n;i++){
        clock_gettime(CLOCK_MONOTONIC, &start);
        // Send the ICMP packet to the destination
        if (sendto(sockfd, packet, PACKET_SIZE, 0, (struct sockaddr *)next_addr, sizeof(*next_addr)) <= 0) {
            printf("Error sending packet: %s\n", strerror(errno));
            exit(1);
        }
        
        // Receive the response from the router
        if ((recv_len = recvfrom(sockfd, recv_buf, MAX_RECV_SIZE, 0, (struct sockaddr *)&recv_addr, &recv_addr_len)) <= 0) {
            printf("*\t");
            fflush(stdout);
            continue;
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        time_taken = get_time_diff(&start, &end, time_unit);
        min_time_taken = min(time_taken, min_time_taken);
        
        // Check if the response is an ICMP echo reply
        // struct iphdr *ip_header = (struct iphdr *)recv_buf;
        // struct icmphdr *icmp_reply = (struct icmphdr *)(recv_buf + (ip_header->ihl * 4));
        // printf("\n");
        print_ip_header(recv_buf);
        print_icmp_header(recv_buf + IP_HDR_SIZE);
        // print_payload(recv_buf + IP_HDR_SIZE + ICMP_HDR_SIZE, recv_len - IP_HDR_SIZE - ICMP_HDR_SIZE);

        if(time_unit == TIME_IN_MICRO)
            printf("%.3lf us\t", time_taken);
        else
            printf("%.3lf ms\t", time_taken);
        fflush(stdout);
        if(time_taken<sleep_time_in_micro && i!=n-1){
            // printf("Sleeping for %ld microseconds\n",sleep_time_in_micro-(long)time_taken);
            usleep(sleep_time_in_micro-(long)time_taken);
        }
    }
    latencies[ttl] = min_time_taken;
    printf("\n");
    if(latencies[ttl-1] < LATENCY_NA && min_time_taken < LATENCY_NA){
        if(time_unit == TIME_IN_MICRO)
            printf("Latency of the Link: %.3lf us\n", (min_time_taken - latencies[ttl-1])/2.0);
        else
            printf("Latency of the Link: %.3lf ms\n", (min_time_taken - latencies[ttl-1])/2.0);
    }
        
    else
        printf("Latency of the Link: NA\n");
    fflush(stdout);
}

// Function to calculate bandwidth of the path
void calc_bandwidth(int sockfd, char* data_packet,struct sockaddr_in* next_addr, int ttl, double *latencies, double *data_latencies, int n, long sleep_time_in_micro, int time_unit){
    struct timespec start, end;
    char recv_buf[DATA_RECV_SIZE];
    struct sockaddr_in recv_addr;
    socklen_t recv_addr_len = sizeof(recv_addr);
    int recv_len;
    double time_taken=0,min_time_taken=LATENCY_NA;
    printf("RTT: \t");
    for(int i=0;i<n;i++){
        clock_gettime(CLOCK_MONOTONIC, &start);
        // Send the ICMP packet to the destination
        if (sendto(sockfd, data_packet, DATA_PACKET_SIZE, 0, (struct sockaddr *)next_addr, sizeof(*next_addr)) <= 0) {
            printf("Error sending packet: %s\n", strerror(errno));
            exit(1);
        }
        
        // Receive the response from the router
        if ((recv_len = recvfrom(sockfd, recv_buf, DATA_RECV_SIZE, 0, (struct sockaddr *)&recv_addr, &recv_addr_len)) <= 0) {
            printf("*\t");
            fflush(stdout);
            continue;
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        // Check if the response is an ICMP echo reply
        // struct iphdr *ip_header = (struct iphdr *)recv_buf;
        // struct icmphdr *icmp_reply = (struct icmphdr *)(recv_buf + (ip_header->ihl * 4));
        // printf("\n");
        print_ip_header(recv_buf);
        print_icmp_header(recv_buf + IP_HDR_SIZE);
        // print_payload(recv_buf + IP_HDR_SIZE + ICMP_HDR_SIZE, recv_len - IP_HDR_SIZE - ICMP_HDR_SIZE);

        printf("\nReceived %d bytes\t",recv_len);
            
        time_taken = get_time_diff(&start, &end, time_unit);
        if(recv_len == DATA_PACKET_SIZE+IP_HDR_SIZE)
            min_time_taken = min(time_taken, min_time_taken);
        else
            printf("Expected packet not returned, hence won't be used to calculate bandwidth\t");
        if(time_unit == TIME_IN_MICRO)
            printf("%.3lf us\t", time_taken);
        else
            printf("%.3lf ms\t", time_taken);
        fflush(stdout);
        if(time_taken<sleep_time_in_micro && i!=n-1){
            // printf("Sleeping for %ld microseconds\n",sleep_time_in_micro-(long)time_taken);
            usleep(sleep_time_in_micro-(long)time_taken);
        }
    }
    data_latencies[ttl] = min_time_taken;
    printf("\n");
    if(data_latencies[ttl-1] < LATENCY_NA && min_time_taken < LATENCY_NA && latencies[ttl-1] < LATENCY_NA && latencies[ttl] < LATENCY_NA){
        double _time_taken = min_time_taken - data_latencies[ttl-1];
        _time_taken -= (latencies[ttl] - latencies[ttl-1]);
        if(_time_taken != 0){
            _time_taken /= 2.0;
            if(time_unit == TIME_IN_MICRO)
                printf("Bandwidth of the Link: %.3lf Mbps\n", ((double)DATA_PACKET_SIZE*8)/(_time_taken));
            else
                printf("Bandwidth of the Link: %.3lf Mbps\n", ((double)DATA_PACKET_SIZE*8)/(_time_taken*1000));
        }
            
    }
    else
        printf("Bandwidth of the Link: NA\n");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <destination IP>\n", argv[0]);
        return 1;
    }
    
    char *dest_ip_str = argv[1];
    
    struct in_addr dest_ip;
    struct hostent *server;
    server = gethostbyname(dest_ip_str);
    if (server == NULL) {
        printf("Invalid destination IP: %s\n", dest_ip_str);
        return 0;
    }
    bcopy((char *)server->h_addr, (char *)&dest_ip.s_addr, server->h_length);
    
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        printf("Error creating raw socket: %s\n", strerror(errno));
        return 1;
    }
    
    int n,t,time_unit;
    char c[10];
    printf("Enter the number of probes to be sent per link for finding latency and bandwidth: ");
    scanf("%d",&n);
    printf("Enter how many seconds apart each probe should be sent: ");
    scanf("%d",&t);
    printf("Enter a output file name to which IP and ICMP Header will be printed for every packet recieved (abc.txt): ");
    scanf("%s",out_file);
    fp = fopen(out_file,"w");
    printf("Do you want to calculate latency in microseconds or milliseconds? (u/m): ");
    scanf("%s",c);
    fflush(stdin);
    if(c[0]=='u' || c[0]=='U')
        time_unit = TIME_IN_MICRO;
    else if(c[0]=='m' || c[0]=='M')
        time_unit = TIME_IN_MILLI;
    else{
        printf("Invalid choice, defaulting to milliseconds\n");
        time_unit = TIME_IN_MILLI;
    }
    // Set the TTL value for the socket
    int ttl = 1;
    char recv_buf[MAX_RECV_SIZE];
    struct sockaddr_in recv_addr;
    socklen_t recv_addr_len = sizeof(recv_addr);
    int recv_len;
    

    double tot_time_taken = 0.0, time_taken = 0.0, min_time_taken = 1e15;
    double latencies[MAX_HOPS+1], data_latencies[MAX_HOPS+1];
    latencies[0] = 0.0;
    data_latencies[0] = 0.0;

    for(int i=1;i<=MAX_HOPS;i++){
        latencies[i] = LATENCY_NA;
        data_latencies[i] = LATENCY_NA;
    }

    // Set the socket timeout to 2 second
    struct timeval timeout;

    
    // Initialize the destination address structure
    struct sockaddr_in dest_addr,next_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    memset(&next_addr, 0, sizeof(next_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr = dest_ip;
    
    next_addr.sin_family = AF_INET;

    // Initialize the packets
    char packet[PACKET_SIZE];
    char data_packet[DATA_PACKET_SIZE];

    // Latency measurement packet
    struct icmphdr *icmp_packet = (struct icmphdr *)(packet);
    icmp_packet->type = ICMP_ECHO;
    icmp_packet->code = 0;
    icmp_packet->un.echo.id = htons(getpid());
    icmp_packet->un.echo.sequence = 0;

    // print_icmp_header(packet);

    add_seq_chars(packet + ICMP_HDR_SIZE, PACKET_SIZE - ICMP_HDR_SIZE);

    // print_payload(packet + ICMP_HDR_SIZE, PACKET_SIZE - ICMP_HDR_SIZE);

    // Calculate the checksum for the ICMP packet
    icmp_packet->checksum = 0;
    icmp_packet->checksum = in_cksum((unsigned short *)icmp_packet, PACKET_SIZE);

    // Bandwidth measurement packet
    struct icmphdr *icmp_data_packet = (struct icmphdr *)(data_packet);
    icmp_data_packet->type = ICMP_ECHO;
    icmp_data_packet->code = 0;
    icmp_data_packet->un.echo.id = htons(getpid());
    icmp_data_packet->un.echo.sequence = 0;

    // print_icmp_header(data_packet);

    add_seq_chars(data_packet + ICMP_HDR_SIZE, DATA_PACKET_SIZE - ICMP_HDR_SIZE);

    // print_payload(data_packet + ICMP_HDR_SIZE, DATA_PACKET_SIZE - ICMP_HDR_SIZE);

    // Calculate the checksum for the ICMP packet
    icmp_data_packet->checksum = 0;
    icmp_data_packet->checksum = in_cksum((unsigned short *)icmp_data_packet, DATA_PACKET_SIZE);


    printf("traceroute to %s (%s), %d hops max, %d byte packets\n", server->h_name, inet_ntoa(dest_ip), MAX_HOPS, PACKET_SIZE);
    int done=0;
    // Loop through the TTL values
    for (ttl = 1; ttl <= MAX_HOPS; ttl++) {
        // Set recv timeout to 2 second
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
            printf("Error setting socket timeout: %s\n", strerror(errno));
            return 1;
        }
        // Set the TTL value for the socket
        if (setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) != 0) {
            printf("Error setting TTL value: %s\n", strerror(errno));
            return 1;
        }
        min_time_taken = LATENCY_NA;
        printf("-----------------------------------------------------------------\n");
        printf("%d.\n", ttl);
        char next_hop[INET_ADDRSTRLEN];
        get_next_hop(sockfd, packet, &dest_addr, &dest_ip, next_hop, time_unit);
        if(strcmp(next_hop,"0.0.0.0")==0){
            printf("Next Hop could not be found\n");
            printf("Hence, Latency and Bandwidth cannot be calculated\n");
            continue;
        }
        printf("Calculating latency and bandwidth for the next hop: %s\n",next_hop);
        next_addr.sin_addr.s_addr = inet_addr(next_hop);
        
        if(strcmp(next_hop,inet_ntoa(dest_ip))==0) done=1;

        long sleep_time_in_micro = t*1000000;
        
        //Set recv timeout to t second
        timeout.tv_sec = t;
        timeout.tv_usec = 0;
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
            printf("Error setting socket timeout: %s\n", strerror(errno));
            return 1;
        }

        printf("Sending %d ICMP packets each %d seconds apart to calculate latencies\n",n,t);

        calc_latency(sockfd, packet, &next_addr, ttl, latencies, n, sleep_time_in_micro, time_unit);

        printf("Sending %d ICMP packets each %d seconds apart to calculate bandwidth\n",n,t);

        calc_bandwidth(sockfd, data_packet, &next_addr, ttl, latencies, data_latencies, n, sleep_time_in_micro, time_unit);
        if(done){
            printf("Destination Reached\n");
            break;
        }   
    }

    // Close the socket
    close(sockfd);

    fclose(fp);

    return 0;



    }