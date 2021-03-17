#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<errno.h>
#include<netinet/in.h>
#include<netinet/ip.h>
#include<netinet/tcp.h>
#include<netinet/ip_icmp.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<sys/time.h>
#include<net/if.h>
#include<sys/ioctl.h>

#define ICMP 0
#define UDP 1
#define TCP 2
#define MAXHOP 64
#define BASE_PORT 33434

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
	
	//fprintf(stderr, "ip: %s\n", IP_buf);
	return IP_buf;
}
unsigned short checksum(char *header, int count)
{
	unsigned short *addr = (unsigned short *)header;
	unsigned long sum = 0;
	unsigned short cksum;

	while (count > 1) {
		sum += *addr;
		addr++;
		count -= 2;
	}
	if (count > 0) {
		sum += *(unsigned char *)addr;
	}
	while (sum >> 16) {
		sum = (sum & 0xffff) + (sum >> 16);
	}
	cksum = (unsigned short)sum;
	cksum = ~cksum;
	return cksum;
}
int check_protocal(char *method) {
	if (method[1] == 'u' || method[1] == 'U')
		return UDP;
	if (method[1] == 't' || method[1] == 'T')
		return TCP;
	return ICMP;
}

void traceroute_icmp(const char *ip);
void traceroute_udp(const char *ip);
void traceroute_tcp(const char *ip);

int main(int argc, char *argv[]){
	char *method = argv[1];
	int protocal = check_protocal(method);

    char *dest = argv[2];
    char *ip = DNSLookup(dest);
    if(ip == NULL){
        printf("traceroute: unknown host %s\n", dest);
        exit(1);
    }
	printf("traceroute to %s (%s), %d hops max\n", dest, ip, MAXHOP);
   	switch(protocal) {
		case ICMP:
			traceroute_icmp(ip);
			break;
		case UDP:
			traceroute_udp(ip);
			break;
		case TCP:
			traceroute_tcp(ip);
			break;

	}
    return 0;
}



void traceroute_icmp(const char *ip) {
	int icmpfd;
    if((icmpfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0){
        printf("Can not open socket with error number %d\n", errno);
        exit(1);
    }
    
    struct sockaddr_in sendAddr;
    sendAddr.sin_port = htons (7);
    sendAddr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &(sendAddr.sin_addr));
    socklen_t sendLength = sizeof(sendAddr);
    
    // Set timeout
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if(setsockopt(icmpfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0){
    	perror("setsockopt timeout error\n");
    	exit(1);
    }
    // TODO

    int finish = 0; // if the packet reaches the destination
	int maxHop = MAXHOP; // maximum hops
    struct icmp sendICMP; 
    struct timeval begin, end; // used to record RTT
    int seq = 0; // increasing sequence number for icmp packet
    int count = 3; // sending count for each ttl
    for(int h = 1; h < maxHop; h++){
        // Set TTL
        if(setsockopt(icmpfd, SOL_IP, IP_TTL, &h, sizeof(h)) < 0){
        	perror("setsockopt TTL error\n");
        	exit(1);
        }
        // TODO
		char hostname[4][128];
        char srcIP[4][32];
        float interval[4] = {};
		int recvCount = 0;
		fprintf(stderr, "%2d ", h);

        for(int c = 0; c < count; c++){
            // Set ICMP Header
            memset(&sendICMP, 0, sizeof(struct icmp));
            sendICMP.icmp_type = ICMP_ECHO;
            sendICMP.icmp_code = 0;
            sendICMP.icmp_cksum = 0;
            //sendICMP.icmp_hun.ih_idseq.icd_id = htons(h);
			seq++;
            sendICMP.icmp_hun.ih_idseq.icd_seq = htons(seq);
			//fprintf(stderr, "\nsend id = %d, seq = %d\n", sendICMP.icmp_hun.ih_idseq.icd_id, sendICMP.icmp_hun.ih_idseq.icd_seq);
            // TODO

            // Checksum
            sendICMP.icmp_cksum = checksum((char*)&sendICMP, sizeof(struct icmp));
            // TODO
            
            // Send the icmp packet to destination
            int ret = sendto(icmpfd, (char*)&sendICMP, sizeof(sendICMP), 0, (struct sockaddr*)&sendAddr, sendLength);
           	if(gettimeofday(&begin, NULL) < 0){
           		perror("gettimeofday error\n");
           	}
           //	fprintf(stderr, "begin : %f\n", (double)begin.tv_sec + (double)((double)begin.tv_usec / 1000000));
            if(ret <= 0){
            	perror("sento error\n");
            	exit(1);
            }
            // TODO
        
            // Recive ICMP reply, need to check the identifier and sequence number
            struct ip *recvIP, *sentIP;
            struct icmp *recvICMP, *sentICMP;
            struct sockaddr_in recvAddr;
            u_int8_t icmpType;
            unsigned int recvLength = sizeof(recvAddr);
            char recvBuf[1500];
            
            memset(&recvAddr, 0, sizeof(struct sockaddr_in));
            // TODO

			while ((ret = recvfrom(icmpfd, &recvBuf, sizeof(recvBuf), 0, (struct sockaddr*)&recvAddr, &recvLength)) > 0) {
				recvIP = (struct ip*)recvBuf;
				recvICMP = (struct icmp*)(recvBuf + (recvIP->ip_hl) * 4);
				sentIP = (struct ip*)(recvBuf + (recvIP->ip_hl) * 4 + 8);
				sentICMP = (struct icmp*)(recvBuf + (recvIP->ip_hl) * 4 + 8 + sentIP->ip_hl * 4);
				icmpType = recvICMP->icmp_type;
				
				unsigned short recvseq = ntohs(*((short*) ( ((char*)sentICMP)+6 )));
				//fprintf(stderr, "\nicmp type = %d, recvseq = %d, seq = %d\n", icmpType, ntohs(recvICMP->icmp_hun.ih_idseq.icd_seq), seq);
				if (icmpType == 0 || icmpType == 11) {
					if (recvseq == seq || ntohs(recvICMP->icmp_hun.ih_idseq.icd_seq) == seq)
						break;
				}
			}
			if(gettimeofday(&end, NULL) < 0){
				perror("gettimeofday error\n");
			}
			//fprintf(stderr, "end : %lf\n", (double)end.tv_sec + (double)((double)end.tv_usec / 1000000));
			if(ret <= 0){
				interval[c] = -1;
				//perror("recvfrom error\n");
				fprintf(stderr, " *");
				//exit(1);
			} else {
				recvCount++;
				
				//fprintf(stderr, "recv ip hl = %d\nsentip hl = %d\n", recvIP->ip_hl, sentIP->ip_hl);
		        // Get source hostname and ip address 
		        getnameinfo((struct sockaddr *)&recvAddr, sizeof(recvAddr), hostname[c], sizeof(hostname[c]), NULL, 0, 0); 
		        strcpy(srcIP[c], inet_ntoa(recvIP->ip_src));
		        
		        if(icmpType == 0){
		            finish = 1;
		        }
				interval[c] = ( (double)end.tv_sec + (double)((double)end.tv_usec / 1000000) ) - ( (double)begin.tv_sec + (double)((double)begin.tv_usec / 1000000) );
				interval[c] *= 1000;
		        // Print the result
		        // TODO
				if (recvCount == 1) {
					fprintf(stderr, "%s (%s)",hostname[0], srcIP[0]);
				}
				fprintf(stderr, "  %.3f ms", interval[c]);
				//fprintf(stderr, "\nh = %d c = %d\n", h, c);
				
				//char type = *(((char*)sentICMP));
				//fprintf(stderr, "\nid = %d seq = %d type = %d\n", ntohs(id), ntohs(seq), sentICMP->icmp_type);
			}
        }    
		fprintf(stderr, "\n");
		if(finish){
            break;
        }
    }
    close(icmpfd);
}
void traceroute_udp(const char *ip)
{
	int sendfd, recvfd;
	if((sendfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        printf("Can not open socket with error number %d\n", errno);
        exit(1);
    }
	if((recvfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0){
        printf("Can not open socket with error number %d\n", errno);
        exit(1);
    }
    
    struct sockaddr_in localAddr;
	bzero(&localAddr,sizeof(localAddr));
    localAddr.sin_port = htons (4097);
    localAddr.sin_family = AF_INET;
	localAddr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(sendfd, (struct sockaddr *)&localAddr, sizeof(localAddr)) != 0) {
        perror("bind error");
        exit(1);
    }
    
    struct sockaddr_in sendAddr;
	bzero(&sendAddr,sizeof(sendAddr));
    sendAddr.sin_family = AF_INET;
    sendAddr.sin_addr.s_addr = inet_addr(ip);
    
    //set timeout
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if(setsockopt(recvfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0){
    	perror("setsockopt timeout error\n");
    	exit(1);
    }
    
    int finish = 0; // if the packet reaches the destination
    int maxHop = MAXHOP; // maximum hops
    struct timeval begin, end; // used to record RTT
    int seq = 0; // increasing sequence number for icmp packet
    int count = 3; // sending count for each ttl
    char sendBuf[32];
    for(int h = 1; h < maxHop; h++){
        // Set TTL
        if(setsockopt(sendfd, IPPROTO_IP, IP_TTL, &h, sizeof(h)) < 0){
        	perror("setsockopt TTL error\n");
        	exit(1);
        }
        // TODO
		char hostname[4][128];
        char srcIP[4][32];
        float interval[4] = {};
		int recvCount = 0;
		fprintf(stderr, "%2d ", h);

        for(int c = 0; c < count; c++){
            // Set ICMP Header
            seq++;
            sendAddr.sin_port = htons(BASE_PORT + seq);
			//int r;
            if( sendto(sendfd, sendBuf, sizeof(sendBuf), 0, (struct sockaddr *) &sendAddr, sizeof(sendAddr) ) < 0){
			//	printf("r=%d\n",r);
            	perror("sendto error\n");
            	exit(1);
            }
           	if(gettimeofday(&begin, NULL) < 0){
           		perror("gettimeofday error\n");
           	}
            // TODO
        
            // Recive ICMP reply, need to check the identifier and sequence number
            struct ip *recvIP, *sentIP;
            struct icmp *recvICMP;
            char *sentUDP;
            struct sockaddr_in recvAddr;
            u_int8_t icmpType;
            unsigned int recvLength = sizeof(recvAddr);
            char recvBuf[1500];
            
            memset(&recvAddr, 0, sizeof(struct sockaddr_in));
            // TODO
            int ret;
			while ((ret = recvfrom(recvfd, &recvBuf, sizeof(recvBuf), 0, (struct sockaddr*)&recvAddr, &recvLength)) > 0) {
				recvIP = (struct ip*)recvBuf;
				recvICMP = (struct icmp*)(recvBuf + (recvIP->ip_hl) * 4);
				sentIP = (struct ip*)(recvBuf + (recvIP->ip_hl) * 4 + 8);
				sentUDP = (char*)(recvBuf + (recvIP->ip_hl) * 4 + 8 + sentIP->ip_hl * 4);

				icmpType = recvICMP->icmp_type;
				//fprintf(stderr, "\nicmp type = %d\n", icmpType);
				unsigned short recvport = ntohs(*((short*) ( ((char*)sentUDP)+2 )));
				if (recvport == BASE_PORT + seq)
					break;
			}
			
			if(gettimeofday(&end, NULL) < 0){
				perror("gettimeofday error\n");
			}
			//fprintf(stderr, "end : %lf\n", (double)end.tv_sec + (double)((double)end.tv_usec / 1000000));
			if(ret <= 0){
				interval[c] = -1;
				//perror("recvfrom error\n");
				fprintf(stderr, " *");
				//exit(1);
			} else {
				recvCount++;
				
				//fprintf(stderr, "recv ip hl = %d\nsentip hl = %d\n", recvIP->ip_hl, sentIP->ip_hl);
		        // Get source hostname and ip address 
		        getnameinfo((struct sockaddr *)&recvAddr, sizeof(recvAddr), hostname[c], sizeof(hostname[c]), NULL, 0, 0); 
		        strcpy(srcIP[c], inet_ntoa(recvIP->ip_src));
		        
		        if(icmpType == 3){
		            finish = 1;
		        }
				interval[c] = ( (double)end.tv_sec + (double)((double)end.tv_usec / 1000000) ) - ( (double)begin.tv_sec + (double)((double)begin.tv_usec / 1000000) );
				interval[c] *= 1000;
		        // Print the result
		        // TODO
				if (recvCount == 1) {
					fprintf(stderr, "%s (%s)",hostname[0], srcIP[0]);
				}
				fprintf(stderr, "  %.3f ms", interval[c]);
				//fprintf(stderr, "\nh = %d c = %d\n", h, c);
				
				//char type = *(((char*)sentICMP));
				//fprintf(stderr, "\nid = %d seq = %d type = %d\n", ntohs(id), ntohs(seq), sentICMP->icmp_type);
			}
        }    
		fprintf(stderr, "\n");
		if(finish){
            break;
        }
    }
}
struct psdhdr{	//pseudoheader for computing tcp checksum
	unsigned int srcIP;
	unsigned int destIP;
	unsigned char zero;
	unsigned char proto;
	unsigned short len;
};
void init_psdhdr(struct psdhdr *phdr, struct ip *iphdr)
{
	phdr->srcIP = iphdr->ip_src.s_addr;
	phdr->destIP = iphdr->ip_dst.s_addr;
	phdr->zero = 0;
	phdr->proto = IPPROTO_TCP;
	phdr->len = htons(sizeof(struct tcphdr));
}

void setsrcIP(struct ip *iphdr)
{
	int fd;
	struct ifreq ifr;
	
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name , "ens33", IFNAMSIZ-1);

	ioctl(fd, SIOCGIFADDR, &ifr);

	close(fd);
	char srcIP[32] = {};
	inet_ntop(AF_INET, &((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr, srcIP, sizeof(srcIP));
	inet_pton(AF_INET, srcIP, &iphdr->ip_src.s_addr);
}
void init_iphdr(struct ip *iphdr, const char *destIP)
{
	iphdr->ip_v = IPVERSION;
	iphdr->ip_hl = sizeof(struct ip) / 4;
	iphdr->ip_tos = 0;
	iphdr->ip_len = htons(sizeof(struct ip) + sizeof(struct tcphdr));
	iphdr->ip_off = 0;
	iphdr->ip_p = IPPROTO_TCP;
	iphdr->ip_sum = 0;
	
	setsrcIP(iphdr);
	inet_pton(AF_INET, destIP, &iphdr->ip_dst.s_addr);
}
void init_tcphdr(struct tcphdr *tcphdr)
{
	tcphdr->source = htons(9431);
	tcphdr->dest = htons(80);

	tcphdr->doff = sizeof(struct tcphdr) / 4;
	tcphdr->syn = 1;
	tcphdr->window = htons(4096);
	tcphdr->check = 0;
	tcphdr->seq = htonl(123);
	tcphdr->ack_seq = 0;
}
unsigned short calcTCPCheckSum(const char* buf) {
	size_t size = sizeof(struct tcphdr) + sizeof(struct psdhdr);
	unsigned int checkSum = 0;
	for (int i = 0; i < size; i += 2) {
		unsigned short first = (unsigned short)buf[i] << 8;
		unsigned short second = (unsigned short)buf[i+1] & 0x00ff;
		checkSum += first + second;
	}
	while (1) {
		unsigned short c = (checkSum >> 16);
		if (c > 0) {
			checkSum = (checkSum << 16) >> 16;
			checkSum += c;
		} else {
			break;
		}
	}
	return ~checkSum;
}
void create_syn_packet(char *packet, const char *destIP)
{
	struct ip iphdr = {};
	init_iphdr(&iphdr, destIP);
	struct tcphdr tcphdr = {};
	init_tcphdr(&tcphdr);
	struct psdhdr phdr = {};
	init_psdhdr(&phdr, &iphdr);
	
	char buf[sizeof(struct psdhdr) + sizeof(struct tcphdr)] = {};
	memcpy(buf, &phdr, sizeof(struct psdhdr));
	memcpy(buf + sizeof(struct psdhdr), &tcphdr, sizeof(struct tcphdr));
	
	for (int i = 0; i < sizeof(buf); i++)
		fprintf(stderr, "%2x ", buf[i] & 0xff);
	fprintf(stderr, "\n");
	tcphdr.check = htons(calcTCPCheckSum(buf));//checksum(buf, sizeof(buf)));
	
	memcpy(packet, &iphdr, sizeof(struct ip));
	memcpy(packet + sizeof(struct ip), &tcphdr, sizeof(struct tcphdr));
}
void traceroute_tcp(const char *ip)
{
	int sendfd, recvfd;
	if((sendfd = socket(AF_INET, SOCK_RAW, IPPROTO_TCP)) < 0){
        printf("Can not open socket with error number %d\n", errno);
        exit(1);
    }
    setsockopt(sendfd, IPPROTO_IP, IP_HDRINCL, "1", sizeof("1"));
	if((recvfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0){
        printf("Can not open socket with error number %d\n", errno);
        exit(1);
    }
    
    struct sockaddr_in sendAddr;
	bzero(&sendAddr,sizeof(sendAddr));
    sendAddr.sin_family = AF_INET;
    sendAddr.sin_addr.s_addr = inet_addr(ip);
    sendAddr.sin_port = htons(80);
    
    //set timeout
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if(setsockopt(recvfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0){
    	perror("setsockopt timeout error\n");
    	exit(1);
    }
    
    int finish = 0; // if the packet reaches the destination
    int maxHop = MAXHOP; // maximum hops
    struct timeval begin, end; // used to record RTT
    int seq = 0; // increasing sequence number for icmp packet
    int count = 3; // sending count for each ttl
    char sendBuf[32];
    int packet_len = sizeof(struct ip) + sizeof(struct tcphdr);
    char *packet = malloc(packet_len);
    create_syn_packet(packet, ip);
    for(int h = 1; h < maxHop; h++){
        // TODO
		char hostname[4][128];
        char srcIP[4][32];
        float interval[4] = {};
		int recvCount = 0;
		fprintf(stderr, "%2d ", h);

        for(int c = 0; c < count; c++){
            seq++;
			struct ip *iphdr = (struct ip*)packet;
			iphdr->ip_id = htonl(12923);
			iphdr->ip_ttl = h;
            if( sendto(sendfd, packet, packet_len, 0, (struct sockaddr *) &sendAddr, sizeof(sendAddr) ) < 0){
			//	printf("r=%d\n",r);
            	perror("sendto error\n");
            	exit(1);
            }
           	if(gettimeofday(&begin, NULL) < 0){
           		perror("gettimeofday error\n");
           	}
            // TODO
        
            // Recive ICMP reply, need to check the identifier and sequence number
            struct ip *recvIP, *sentIP;
            struct icmp *recvICMP;
            char *sentUDP;
            struct sockaddr_in recvAddr;
            u_int8_t icmpType;
            unsigned int recvLength = sizeof(recvAddr);
            char recvBuf[1500];
            
            memset(&recvAddr, 0, sizeof(struct sockaddr_in));
            // TODO
            int ret;
			while ((ret = recvfrom(recvfd, &recvBuf, sizeof(recvBuf), 0, (struct sockaddr*)&recvAddr, &recvLength)) > 0) {
				recvIP = (struct ip*)recvBuf;
				recvICMP = (struct icmp*)(recvBuf + (recvIP->ip_hl) * 4);
				sentIP = (struct ip*)(recvBuf + (recvIP->ip_hl) * 4 + 8);

				icmpType = recvICMP->icmp_type;
				//fprintf(stderr, "\nicmp type = %d\n", icmpType);
				unsigned short id = sentIP->ip_id;
				//printf("id = %d\n", id);
				if (id == seq)
					break;
			}
			
			if(gettimeofday(&end, NULL) < 0){
				perror("gettimeofday error\n");
			}
			//fprintf(stderr, "end : %lf\n", (double)end.tv_sec + (double)((double)end.tv_usec / 1000000));
			if(ret <= 0){
				interval[c] = -1;

				//perror("recvfrom error\n");
				fprintf(stderr, " *");
				//exit(1);
			} else {
				recvCount++;
				
				//fprintf(stderr, "recv ip hl = %d\nsentip hl = %d\n", recvIP->ip_hl, sentIP->ip_hl);
		        // Get source hostname and ip address 
		        getnameinfo((struct sockaddr *)&recvAddr, sizeof(recvAddr), hostname[c], sizeof(hostname[c]), NULL, 0, 0); 
		        strcpy(srcIP[c], inet_ntoa(recvIP->ip_src));
		        
		        if(icmpType == 3){
		            finish = 1;
		        }
				interval[c] = ( (double)end.tv_sec + (double)((double)end.tv_usec / 1000000) ) - ( (double)begin.tv_sec + (double)((double)begin.tv_usec / 1000000) );
				interval[c] *= 1000;
		        // Print the result
		        // TODO
				if (recvCount == 1) {
					fprintf(stderr, "%s (%s)",hostname[0], srcIP[0]);
				}
				fprintf(stderr, "  %.3f ms", interval[c]);
			}
        }    
		fprintf(stderr, "\n");
		if(finish){
            break;
        }
    }
    free(packet);
}
