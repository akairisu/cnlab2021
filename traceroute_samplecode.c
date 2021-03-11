#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<errno.h>
#include<netinet/in.h>
#include<netinet/ip.h>
#include<netinet/ip_icmp.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<sys/time.h>

/*char *SearchDNSServer(){
	FILE *fp = fopen("/etc/resolv.conf", "r");
	char *DNSServer, line[100], *start;
	DNSServer = (char*)malloc(sizeof(char) * 100);
	while(fgets(line, 100, fp)){
		if(line[0] == '#'){
			continue;		
		}
		else{
			if(strncmp(line, "nameserver", 10) == 0){
				start = strchr(line, ' ');
				strcpy(DNSServer, start);
			}
			else{
				continue;			
			}
		}
	}
	return DNSServer;
}*/

char *DNSLookup(char *host){
	char *IP_buf;
	struct hostent *host_entry;
	
	if(host == NULL){
		perror("host name error!\n");
		exit(1);
	}
	
	host_entry = gethostbyname(host);
	if(host_entry == NULL){
		perror("gethostbyname error!\n");
		exit(1);
	}
	
	IP_buf = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));
	
	fprintf(stderr, "ip: %s\n", IP_buf);
	return IP_buf;
}

int main(int argc, char *argv[]){
    char *dest = argv[1];
    char *ip = DNSLookup(dest);
    if(ip == NULL){
        printf("traceroute: unknown host %s\n", dest);
        exit(1);
    }
    int icmpfd;
    if((icmpfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0){
        printf("Can not open socket with error number %d\n", errno);
        exit(1);
    }
    
    struct sockaddr_in sendAddr;
    sendAddr.sin_port = htons (7);
    sendAddr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &(sendAddr.sin_addr));
    
    // Set timeout
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 1000;
    if(setsockopt(icmpfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0){
    	perror("setsockopt timeout error\n");
    	exit(1);
    }
    // TODO

    int finish = 0; // if the packet reaches the destination
    int maxHop = 64; // maximum hops
    struct icmp sendICMP; 
    struct timeval begin, end; // used to record RTT
    int seq = 0; // increasing sequence number for icmp packet
    int count = 3; // sending count for each ttl
    printf("traceroute to %s (%s), %d hops max\n", dest, ip, maxHop);
    for(int h = 1; h < maxHop; h++){
        // Set TTL
        if(setsockopt(icmpfd, SOL_IP, IP_TTL, &h, sizeof(h)) < 0){
        	perror("setsockopt TTL error\n");
        	exit(1);
        }
        // TODO

        for(int c = 0; c < count; c++){
            // Set ICMP Header
            struct icmphdr header;
            memset(&header, 0, sizeof(struct icmphdr));
            header.type = ICMP_ECHO;
            header.code = 0;
            header.checksum = 0;
            header.un.echo.id = 0;
            header.un.echo.sequence = 0;
            // TODO

            // Checksum
            checksum(&header);
            // TODO
            
            // Send the icmp packet to destination
            int ret = sendto();
            // TODO
        
            // Recive ICMP reply, need to check the identifier and sequence number
            struct ip *recvIP;
            struct icmp *recvICMP;
            struct sockaddr_in recvAddr;
            u_int8_t icmpType;
            unsigned int recvLength = sizeof(recvAddr);
            char recvBuf[1500];
            char hostname[4][128];
            char srcIP[4][32];
            float interval[4] = {};
            memset(&recvAddr, 0, sizeof(struct sockaddr_in));
            // TODO

            // Get source hostname and ip address 
            getnameinfo((struct sockaddr *)&recvAddr, sizeof(recvAddr), hostname[c], sizeof(hostname[c]), NULL, 0, 0); 
            strcpy(srcIP[c], inet_ntoa(recvIP->ip_src));
            if(icmpType == 0){
                finish = 1;
            }

            // Print the result
            // TODO
        }    
        if(finish){
            break;
        }
    }
    close(icmpfd);
    return 0;
}
