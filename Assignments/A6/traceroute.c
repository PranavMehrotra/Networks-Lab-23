#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <netdb.h>

#define PACKET_SIZE 64
#define MAX_HOPS 30

unsigned short in_cksum(unsigned short *addr, int len) {
    unsigned int sum = 0;
    while (len > 1) {
        sum += *addr++;
        len -= 2;
    }
    if (len == 1) {
        sum += *(unsigned char *)addr;
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
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
    if (setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) != 0) {
        printf("Error setting TTL value: %s\n", strerror(errno));
        return 1;
    }
    
    // Set the socket timeout to 1 second
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
        printf("Error setting socket timeout: %s\n", strerror(errno));
        return 1;
    }
    
    // Initialize the destination address structure
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr = dest_ip;
    
    // Initialize the ICMP packet
    char packet[PACKET_SIZE];
    struct icmphdr *icmp_packet = (struct icmphdr *)packet;
    icmp_packet->type = ICMP_ECHO;
    icmp_packet->code = 0;
    
    int seq = 0;
    
    // Loop through the TTL values
    for (ttl = 1; ttl <= MAX_HOPS; ttl++) {
        // Set the TTL value for the socket
        if (setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) != 0) {
            printf("Error setting TTL value: %s\n", strerror(errno));
            return 1;
        }
        
        // Initialize the source address structure
        struct sockaddr_in src_addr;
        memset(&src_addr, 0, sizeof(src_addr));
        src_addr.sin_family = AF_INET;
        src_addr.sin_addr.s_addr = INADDR_ANY;
        
        // Bind the socket to the source address
        if (bind(sockfd, (struct sockaddr *)&src_addr, sizeof(src_addr)) != 0) {
            printf("Error binding socket: %s\n", strerror(errno));
            return 1;
        }
        
        // Set the ICMP sequence number
        // icmp_packet->icmp_seq = seq++;
        
        // Calculate the checksum for the ICMP packet
        icmp_packet->checksum = 0;
        icmp_packet->checksum = htons(~(htons(in_cksum((unsigned short *)icmp_packet, PACKET_SIZE))));
        
        printf("%d ", ttl);

        // Send the ICMP packet to the destination
        if (sendto(sockfd, packet, PACKET_SIZE, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) <= 0) {
            printf("Error sending packet: %s\n", strerror(errno));
            return 1;
        }
        
        // Receive the response from the router
        char recv_buf[PACKET_SIZE];
        struct sockaddr_in recv_addr;
        socklen_t recv_addr_len = sizeof(recv_addr);
        if (recvfrom(sockfd, recv_buf, PACKET_SIZE, 0, (struct sockaddr *)&recv_addr, &recv_addr_len) <= 0) {
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
        if (icmp_reply->type == ICMP_ECHOREPLY) {
            printf("Done\n");
            break;
        }
        printf("Source addr: %d ",ip_header->saddr);
        if(ip_header->saddr == dest_ip.s_addr){
            printf("Done\n");
            break;
        }
        
        printf("\n");
    }

    // Close the socket
    close(sockfd);

    return 0;



    }