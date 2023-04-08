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
#define START_ASCII 72
#define NUM_PING_TRY 5
#define TIME_IN_MICRO 1
#define TIME_IN_MILLI 0
#define ROUTE_PROBE_TIME 1000000
#define LATENCY_NA 1e12

const int MAX_HOPS = 30;


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
    printf("IP header:\n");
    printf("  Check: %d\n", ip_hdr->check);
    printf("  Version: %d\n", ip_hdr->version);
    printf("  Source: %s\n", inet_ntoa(*(struct in_addr *)&ip_hdr->saddr));
    printf("  Destination: %s\n", inet_ntoa(*(struct in_addr *)&ip_hdr->daddr));
    printf("  Header length: %d\n", ip_hdr->ihl);
    printf("  Total length: %d\n", ntohs(ip_hdr->tot_len));
    printf("  TTL: %d\n", ip_hdr->ttl);
    printf("  ID: %d\n", ntohs(ip_hdr->id));
    printf("  Protocol: %d\n", ip_hdr->protocol);
    printf("  Fragment offset: %d\n", ip_hdr->frag_off);
}

void print_icmp_header(unsigned char *buf){
    struct icmphdr *icmp_hdr = (struct icmphdr *)(buf);
    printf("ICMP header:\n");
    printf("  Type: %d\n", icmp_hdr->type);
    printf("  Code: %d\n", icmp_hdr->code);
    printf("  ID: %d\n", ntohs(icmp_hdr->un.echo.id));
    printf("  Sequence: %d\n", ntohs(icmp_hdr->un.echo.sequence));
    printf("  Checksum: %d\n", icmp_hdr->checksum);
}

void print_payload(unsigned char *buf, int len) {
    int i;
    printf("Payload:\n");
    printf("Length: %d bytes\n", len);
    for (i = 0; i < len; i++) {
        printf("%02x ", buf[i]);
        if(i%8==7) printf("\n");
    }
    printf("\n\n");
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
void add_seq_nums(unsigned char *buf, int start, int num_bytes) {
    int i;
    for (i = 0; i < num_bytes; i++) {
        buf[i] = start + i;
    }
}

void get_next_hop(int sockfd, char* packet,struct sockaddr_in* dest_addr,struct in_addr* dest_ip, char *next_hop){
    char recv_buf[MAX_RECV_SIZE];
    struct sockaddr_in recv_addr;
    socklen_t recv_addr_len = sizeof(recv_addr);
    char next_router[INET_ADDRSTRLEN];
    int recv_len;
    printf("Sending %d ICMP packets to finalise the next hop\n",NUM_PING_TRY);
    struct timespec start, end;
    int sleep_time_in_micro = ROUTE_PROBE_TIME;
    long diff=0;
    memset(next_hop, 0, INET_ADDRSTRLEN);
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
            strcpy(next_hop,"0.0.0.0");
            continue;
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        diff = get_time_diff(&start, &end, TIME_IN_MICRO);
        // Print the IP address of the router that responded
        if (inet_ntop(AF_INET, &recv_addr.sin_addr, next_router, INET_ADDRSTRLEN) == NULL) {
            printf("Invalid Source IP address\n");
            continue;
        }
        printf("%s\t",next_router);
        strcpy(next_hop,next_router);
        
        if(diff<sleep_time_in_micro){
            // printf("Sleeping for %ld microseconds\n",sleep_time_in_micro-diff);
            usleep(sleep_time_in_micro-diff);
        }
    }
    printf("\nNext hop is %s\n",next_hop);
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
    
    int n,t;
    printf("Enter the number of probes to be sent per link for finding latency and bandwidth: ");
    scanf("%d",&n);
    printf("Enter how many seconds apart each probe should be sent: ");
    scanf("%d",&t);


    // Set the TTL value for the socket
    int ttl = 1;
    char recv_buf[MAX_RECV_SIZE];
    struct sockaddr_in recv_addr;
    socklen_t recv_addr_len = sizeof(recv_addr);
    char router_ip_str[INET_ADDRSTRLEN];
    int recv_len;
    
    struct timespec tot_start, tot_end, start, end;
    double tot_time_taken = 0.0, time_taken = 0.0, min_time_taken = 1e15;
    double latencies[MAX_HOPS+1];
    latencies[0] = 0.0;

    for(int i=1;i<=MAX_HOPS;i++){
        latencies[i] = LATENCY_NA;
    }

    // Set the socket timeout to 1 second
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
        printf("Error setting socket timeout: %s\n", strerror(errno));
        return 1;
    }

    // Set socket to not attach the IP header
    // int one = 1;
    // if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) != 0) {
    //     printf("Error setting socket option: %s\n", strerror(errno));
    //     return 1;
    // }
    
    // Initialize the destination address structure
    struct sockaddr_in dest_addr,next_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    memset(&next_addr, 0, sizeof(next_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr = dest_ip;
    
    next_addr.sin_family = AF_INET;

    // Initialize the packet
    char packet[PACKET_SIZE];
    // struct iphdr *ip_packet = (struct iphdr *)packet;
    // ip_packet->version = 4;
    // ip_packet->ihl = 5;
    // ip_packet->tos = 0;
    // ip_packet->tot_len = htons(PACKET_SIZE);
    // ip_packet->id = 0;
    // ip_packet->frag_off = 0;
    // ip_packet->ttl = 1;
    // ip_packet->protocol = IPPROTO_ICMP;
    // ip_packet->check = 0;
    // char *host = get_ip_addr();
    // ip_packet->saddr = inet_addr(host);
    // free(host);
    // ip_packet->daddr = dest_ip.s_addr;
    // ip_packet->check = in_cksum((unsigned short *)ip_packet, IP_HDR_SIZE);

    // print_ip_header(packet);

    // struct icmphdr *icmp_packet = (struct icmphdr *)(packet + IP_HDR_SIZE);
    struct icmphdr *icmp_packet = (struct icmphdr *)(packet);
    icmp_packet->type = ICMP_ECHO;
    icmp_packet->code = 0;
    icmp_packet->un.echo.id = htons(getpid());
    icmp_packet->un.echo.sequence = 0;

    print_icmp_header(packet);

    add_seq_nums(packet + ICMP_HDR_SIZE, START_ASCII, PACKET_SIZE - ICMP_HDR_SIZE);

    print_payload(packet + ICMP_HDR_SIZE, PACKET_SIZE - ICMP_HDR_SIZE);

    // Calculate the checksum for the ICMP packet
    icmp_packet->checksum = 0;
    icmp_packet->checksum = in_cksum((unsigned short *)icmp_packet, PACKET_SIZE);

    clock_gettime(CLOCK_MONOTONIC, &tot_start);

    printf("traceroute to %s (%s), %d hops max, %d byte packets\n", server->h_name, inet_ntoa(dest_ip), MAX_HOPS, PACKET_SIZE);
    int done=0;
    // Loop through the TTL values
    for (ttl = 1; ttl <= MAX_HOPS; ttl++) {
        // Set the TTL value for the socket
        if (setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) != 0) {
            printf("Error setting TTL value: %s\n", strerror(errno));
            return 1;
        }
        min_time_taken = LATENCY_NA;
        // ip_packet->ttl = ttl;
        // ip_packet->id = htons(ttl);
        // ip_packet->check = 0;
        // ip_packet->check = in_cksum((unsigned short *)ip_packet, IP_HDR_SIZE);
        printf("%d.\n", ttl);
        char next_hop[INET_ADDRSTRLEN];
        get_next_hop(sockfd, packet, &dest_addr, &dest_ip, next_hop);
        if(strcmp(next_hop,"0.0.0.0")==0){
            printf("Next Hop could not be found\n");
            printf("Hence, Latency and Bandwidth cannot be calculated\n");
            continue;
        }
        printf("%s\n",next_hop);
        next_addr.sin_addr.s_addr = inet_addr(next_hop);
        long sleep_time_in_micro = t*1000000;
        for(int i=0;i<n;i++){
            // Set the TTL value for the socket
            if (setsockopt(sockfd, IPPROTO_IP, IP_TTL, &MAX_HOPS, sizeof(MAX_HOPS)) != 0) {
                printf("Error setting TTL value: %s\n", strerror(errno));
                return 1;
            }
            clock_gettime(CLOCK_MONOTONIC, &start);
            // Send the ICMP packet to the destination
            if (sendto(sockfd, packet, PACKET_SIZE, 0, (struct sockaddr *)&next_addr, sizeof(next_addr)) <= 0) {
                printf("Error sending packet: %s\n", strerror(errno));
                return 1;
            }
            
            // Receive the response from the router
            if ((recv_len = recvfrom(sockfd, recv_buf, MAX_RECV_SIZE, 0, (struct sockaddr *)&recv_addr, &recv_addr_len)) <= 0) {
                printf("*\t");
                continue;
            }
            clock_gettime(CLOCK_MONOTONIC, &end);
            time_taken = get_time_diff(&start, &end, TIME_IN_MICRO);
            min_time_taken = min(time_taken, min_time_taken);
            // Print the IP address of the router that responded
            if (inet_ntop(AF_INET, &recv_addr.sin_addr, router_ip_str, INET_ADDRSTRLEN) == NULL) {
                printf("Invalid Source IP address\n");
                continue;
            }
            
            // Check if the response is an ICMP echo reply
            struct iphdr *ip_header = (struct iphdr *)recv_buf;
            struct icmphdr *icmp_reply = (struct icmphdr *)(recv_buf + (ip_header->ihl * 4));
            // printf("\n");
            // print_ip_header(recv_buf);
            // print_icmp_header(recv_buf + IP_HDR_SIZE);
            // print_payload(recv_buf + IP_HDR_SIZE + ICMP_HDR_SIZE, recv_len - IP_HDR_SIZE - ICMP_HDR_SIZE);
            if (ip_header->saddr == dest_ip.s_addr) {
                done=1;
            }
            printf("%.3lf us\t", time_taken);
            if(time_taken<sleep_time_in_micro){
                printf("Sleeping for %ld microseconds\n",sleep_time_in_micro-(long)time_taken);
                usleep(sleep_time_in_micro-time_taken);
            }
        }
        latencies[ttl] = min_time_taken;
        printf("\n");
        if(latencies[ttl-1] < LATENCY_NA)
            printf("Latency of the Link: %.3lf us\n", min_time_taken - latencies[ttl-1]); //## Change to max(0, min_time_taken - latencies[ttl-1])
        else
            printf("Latency of the Link: NA\n");
        if(done){
            printf("Destination Reached\n");
            break;
        }   
    }

    // Close the socket
    close(sockfd);

    return 0;



    }