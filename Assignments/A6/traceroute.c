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
#include <netdb.h>
#include <ifaddrs.h>

#define PACKET_SIZE 56
#define ICMP_HDR_SIZE 8
#define IP_HDR_SIZE 20
#define MAX_HOPS 10
#define START_ASCII 72


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
    struct icmphdr *icmp_hdr = (struct icmphdr *)(buf + sizeof(struct iphdr));
    printf("ICMP header:\n");
    printf("  Type: %d\n", icmp_hdr->type);
    printf("  Code: %d\n", icmp_hdr->code);
    printf("  Checksum: %d\n", icmp_hdr->checksum);
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

// Function to add sequential binary numbers to the ICMP payload, given the starting number and the number of bytes to add
void add_seq_nums(unsigned char *buf, int start, int num_bytes) {
    int i;
    for (i = 0; i < num_bytes; i++) {
        buf[i] = start + i;
    }
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
    // if (inet_pton(AF_INET, dest_ip_str, &dest_ip) != 1) {
    //     printf("Invalid destination IP: %s\n", dest_ip_str);
    //     return 1;
    // }
    
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        printf("Error creating raw socket: %s\n", strerror(errno));
        return 1;
    }
    
    // Set the TTL value for the socket
    int ttl = 1;
    // if (setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) != 0) {
    //     printf("Error setting TTL value: %s\n", strerror(errno));
    //     return 1;
    // }
    
    // Set the socket timeout to 1 second
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
        printf("Error setting socket timeout: %s\n", strerror(errno));
        return 1;
    }

    // Set socket to not attach the IP header
    int one = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) != 0) {
        printf("Error setting socket option: %s\n", strerror(errno));
        return 1;
    }
    
    // Initialize the destination address structure
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr = dest_ip;
    
    // Initialize the packet
    char packet[PACKET_SIZE];
    struct iphdr *ip_packet = (struct iphdr *)packet;
    ip_packet->version = 4;
    ip_packet->ihl = 5;
    ip_packet->tos = 0;
    ip_packet->tot_len = htons(PACKET_SIZE);
    ip_packet->id = 0;
    ip_packet->frag_off = 0;
    ip_packet->ttl = 1;
    ip_packet->protocol = IPPROTO_ICMP;
    ip_packet->check = 0;
    char *host = get_ip_addr();
    ip_packet->saddr = inet_addr(host);
    free(host);
    ip_packet->daddr = dest_ip.s_addr;
    ip_packet->check = in_cksum((unsigned short *)ip_packet, IP_HDR_SIZE);

    print_ip_header(packet);



    struct icmphdr *icmp_packet = (struct icmphdr *)(packet + IP_HDR_SIZE);
    icmp_packet->type = ICMP_ECHO;
    icmp_packet->code = 0;

    print_icmp_header(packet);

    add_seq_nums(packet + IP_HDR_SIZE + ICMP_HDR_SIZE, START_ASCII, PACKET_SIZE - IP_HDR_SIZE - ICMP_HDR_SIZE);

    printf("Payload:\n");
        for (int i = (sizeof(struct iphdr) + sizeof(struct icmphdr)); i < PACKET_SIZE; i++) {
            printf("%02x ", (unsigned char)packet[i]);
            if (i % 16 == 7) {
                printf("\n");
            }
        }
        printf("\n\n");

    printf("traceroute to %s (%s), %d hops max, %d byte packets\n", server->h_name, inet_ntoa(dest_ip), MAX_HOPS, PACKET_SIZE);

    // Loop through the TTL values
    for (ttl = 1; ttl <= MAX_HOPS; ttl++) {
        // Set the TTL value for the socket
        // if (setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) != 0) {
        //     printf("Error setting TTL value: %s\n", strerror(errno));
        //     return 1;
        // }
        ip_packet->ttl = ttl;
        ip_packet->id = htons(ttl);
        ip_packet->check = 0;
        ip_packet->check = in_cksum((unsigned short *)ip_packet, IP_HDR_SIZE);
        
        // Calculate the checksum for the ICMP packet
        icmp_packet->checksum = 0;
        icmp_packet->checksum = in_cksum((unsigned short *)icmp_packet, ICMP_HDR_SIZE);
        
        printf("%d ", ttl);

        // Send the ICMP packet to the destination
        if (sendto(sockfd, packet, PACKET_SIZE, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) <= 0) {
            printf("Error sending packet: %s\n", strerror(errno));
            return 1;
        }
        
        // Receive the response from the router
        char recv_buf[PACKET_SIZE+1];
        struct sockaddr_in recv_addr;
        socklen_t recv_addr_len = sizeof(recv_addr);
        int recv_len;
        if ((recv_len = recvfrom(sockfd, recv_buf, PACKET_SIZE, 0, (struct sockaddr *)&recv_addr, &recv_addr_len)) <= 0) {
            printf("**\n");
            continue;
        }
        
        // Print the IP address of the router that responded
        char router_ip_str[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &recv_addr.sin_addr, router_ip_str, INET_ADDRSTRLEN) == NULL) {
            printf("*\n");
            continue;
        }
        printf("%s ", router_ip_str);
        
        // Check if the response is an ICMP echo reply
        struct iphdr *ip_header = (struct iphdr *)recv_buf;
        struct icmphdr *icmp_reply = (struct icmphdr *)(recv_buf + (ip_header->ihl * 4));
        // printf("\n");
        print_ip_header(recv_buf);
        print_icmp_header(recv_buf);
        printf("Payload:\n");
        for (int i = (sizeof(struct iphdr) + sizeof(struct icmphdr)); i < recv_len; i++) {
            printf("%02x ", (unsigned char)recv_buf[i]);
            if (i % 16 == 7) {
                printf("\n");
            }
        }
        printf("\n\n");
        if (icmp_reply->type == ICMP_ECHOREPLY) {
            printf("Done\n");
            break;
        }
        if(ip_header->saddr == dest_ip.s_addr){
            printf("Done 2\n");
            break;
        }
        
        // printf("\n");
    }

    // Close the socket
    close(sockfd);

    return 0;



    }