#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <iconv.h>

#define MAX_SIZE 4096

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


int main(){
    int			sockfd ;
	struct sockaddr_in	serv_addr;

	int i,n;
    char buf[MAX_SIZE];
	/* Opening a socket is exactly similar to the server process */
	if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
		perror("Unable to create socket\n");
		exit(0);
	}
    struct iphdr *ip_hdr;
    struct icmphdr *icmp_hdr;
    ip_hdr = (struct iphdr *)buf;
    icmp_hdr = (struct icmphdr *)(buf + sizeof(struct iphdr));
    while(1){
        int serv_len = sizeof(serv_addr);
        n = recvfrom(sockfd, buf, MAX_SIZE, 0, (struct sockaddr *)&serv_addr, &serv_len);
        // printf("Server IP: %s\n", inet_ntoa(serv_addr.sin_addr));
        // printf("Server Port: %d\n\n", ntohs(serv_addr.sin_port));
        // printf("IP header:\n");
        // printf("  Check: %d\n", ip_hdr->check);
        // printf("  Version: %d\n", ip_hdr->version);
        // printf("  Source: %s\n", inet_ntoa(*(struct in_addr *)&ip_hdr->saddr));
        // printf("  Destination: %s\n", inet_ntoa(*(struct in_addr *)&ip_hdr->daddr));
        // printf("  Header length: %d\n", ip_hdr->ihl);
        // printf("  Total length: %d\n", ip_hdr->tot_len);
        // printf("  TTL: %d\n", ip_hdr->ttl);
        // printf("  ID: %d\n", ip_hdr->id);
        // printf("  Protocol: %d\n", ip_hdr->protocol);
        // printf("  Fragment offset: %d\n", ip_hdr->frag_off);


        // printf("ICMP header:\n");
        // printf("  Type: %d\n", icmp_hdr->type);
        // printf("  Code: %d\n", icmp_hdr->code);

        print_ip_header(buf);
        print_icmp_header(buf);

        printf("Payload:\n");
        for (int i = (sizeof(struct iphdr) + sizeof(struct icmphdr)); i < n; i++) {
            printf("%02x ", (unsigned char)buf[i]);
            if (i % 16 == 4) {
                printf("\n");
            }
        }
        // printf("Payload:\n");

        // char *src_ptr = buf + sizeof(struct iphdr) + sizeof(struct icmphdr);
        // size_t src_len = n - sizeof(struct iphdr) - sizeof(struct icmphdr);
        // char dest_buf[MAX_SIZE];
        // char *dest_ptr = dest_buf;
        // size_t dest_len = MAX_SIZE;

        // iconv_t conv = iconv_open("UTF-8", "ISO-8859-1");
        // if (conv == (iconv_t)-1) {
        //     perror("iconv_open");
        //     exit(EXIT_FAILURE);
        // }

        // if (iconv(conv, &src_ptr, &src_len, &dest_ptr, &dest_len) == (size_t)-1) {
        //     perror("iconv");
        //     exit(EXIT_FAILURE);
        // }

        // *dest_ptr = '\0';

        // printf("%s\n\n", dest_buf);

        // iconv_close(conv);
        printf("\n\n");
        // printf("\n\n\n");

    }

	
	return 0;

}