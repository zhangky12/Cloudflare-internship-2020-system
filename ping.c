#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <netinet/ip_icmp.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <signal.h>

#define PACKAGE_SIZE 64

//statistics variable
int send_num = 0, recv_num = 0;
//record the start_time
struct timeval start_time;

//using ifconf to get local ip
char* get_local_ip_using_ifconf()  
{
	char* str_ip = (char*)malloc(sizeof(char) * 20);
    int sock_fd, intrface;
    struct ifreq buf[INET_ADDRSTRLEN];
    struct ifconf ifc;
    char *local_ip = NULL;

    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0)
    {
        ifc.ifc_len = sizeof(buf);
        ifc.ifc_buf = (caddr_t)buf;
        if (!ioctl(sock_fd, SIOCGIFCONF, (char *)&ifc))
        {
            intrface = ifc.ifc_len/sizeof(struct ifreq);
            while (intrface-- > 0)
            {
                if (!(ioctl(sock_fd, SIOCGIFADDR, (char *)&buf[intrface])))
                {
                    local_ip = NULL;
                    local_ip = inet_ntoa(((struct sockaddr_in*)(&buf[intrface].ifr_addr))->sin_addr);
                    if(local_ip)
                    {
                        strcpy(str_ip, local_ip);
                        if(strcmp("127.0.0.1", str_ip))
                        {
                            break;
                        }
                    }

                }
            }
        }
        close(sock_fd);
    }
    return str_ip;
}

/*
* return the ip address if host provided by DNS name
*/
char* toip(char* address)
{
	struct hostent* h = gethostbyname(address);
	return inet_ntoa(*(struct in_addr *)h->h_addr);
}

//try to get destination and ttl-info from argument
void parse_argvs(char** argv, char* dst, char* src, int* ttl)
{
	//invalid argument
	if(!(argv[1]))
	{
		printf("Usage ./ping hostname/ip_address ttl(optional)\n");
		exit(1);
	}
	//argument 1 exist
	else if (argv[1])
	{
		//get destination
		strncpy(dst, argv[1], strlen(argv[1]) + 1);
		char* local_ip = get_local_ip_using_ifconf();
		strncpy(src, local_ip, 15);
		free(local_ip);
		
		//ttl exists
		if(*(argv + 2))
		{
			*ttl = atoi(argv[2]);
		}
	}
	return;
}


//all data is 16bit
//sum all data, because sizeof(unsigned short) == 2, and len is calculated according to sizeof(unsigned char)
//so nleft -= 2
unsigned short in_cksum(unsigned short *addr, int len)
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = addr;
    unsigned short answer = 0;

    while(nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }
    
    if(nleft  == 1)
    {
        *(unsigned char *)(&answer) = *(unsigned char *)w;
        sum += answer;
    }
    
    //add high 16bit to low 16bit
    //treat carry
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    //negative
    answer = ~sum;

    return answer;
}

//calculate the different between start-time and end-time, store the value in out
void calc_time_used(struct timeval *start, struct timeval *end, struct timeval * out)
{
    int sec = start->tv_sec - end->tv_sec;
    int usec = start->tv_usec - end->tv_usec;
    if(usec < 0)
    {
        out->tv_sec = sec - 1;
        out->tv_usec = 1000000 + usec;
    }
    else
    {
        out->tv_sec = sec;
        out->tv_usec = usec;
    }
}

//statistics the ping program
void statistics()
{
	struct timeval end_time, out;
    gettimeofday(&end_time, NULL);
    calc_time_used(&end_time, &start_time, &out);
    double time_used = out.tv_sec * 1000 + (double)out.tv_usec / 1000;
    printf("-------statistics-------\n");
    printf("%d packets transmitted, %d receive, %lf%% lost, time %.3lfms\n",
           send_num,
           recv_num,
           1.0 * (send_num - recv_num) / send_num * 100.0,
           time_used);
    exit(0);
}

int main(int argc, char* argv[])
{
	char dst_addr[20];
	char src_addr[20];
	struct iphdr *ip, *ip_reply;
	struct icmp *Icmp, *icmp_reply;
	struct sockaddr_in connection;
	char* packet = malloc(PACKAGE_SIZE);
	char* buffer = malloc(PACKAGE_SIZE);
	int sockfd;
	int ttl = 64; //default ttl
	
	if (getuid() != 0)
	{
		fprintf(stderr, "%s: root privelidges needed\n", argv[0]);
		exit(1);
	}
	
	parse_argvs(argv, dst_addr, src_addr, &ttl);
	strncpy(dst_addr, toip(dst_addr), 20);
	printf("Source address: %s\n", src_addr);
	printf("Destination address: %s\n", dst_addr);


	//get ip_head and icmp_head
	ip = (struct iphdr*) packet;
	Icmp = (struct icmp*) (packet + sizeof(struct iphdr));

	//initialize ip_head
	ip->ihl = 5;
	ip->version = 4;
	ip->tos = 0;
	ip->tot_len = PACKAGE_SIZE;
	ip->id = getpid();
	ip->frag_off = 0;
	ip->ttl = ttl;
	ip->protocol = IPPROTO_ICMP;
	ip->saddr = inet_addr(src_addr);
	ip->daddr = inet_addr(dst_addr);
	ip->check = 0;
	ip->check = in_cksum((unsigned short *)ip, sizeof(struct iphdr));

	//create raw socket
	if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1)
	{
		perror("socket");
		exit(EXIT_FAILURE);
	}

	//let kernel does not attempt to automatically add a default ip header to the packet
	int on = 1;
	setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(int));
	
	
	signal(SIGINT, statistics);
	
	//infinite loop
	while(1)
	{
		// create the icmp packet is 
		Icmp->icmp_type = ICMP_ECHO;
		Icmp->icmp_code = 0;
		Icmp->icmp_id = getpid();
		Icmp->icmp_seq = ++send_num;
		
		//copy to time the packet creation(the time of send)
		struct timeval *tvsend = (struct timeval*)(Icmp->icmp_data);
		gettimeofday(tvsend, NULL);
		
		//calc the checksum
		Icmp-> icmp_cksum = 0;
		Icmp-> icmp_cksum = in_cksum((unsigned short *)Icmp, PACKAGE_SIZE - sizeof(struct iphdr));
		
		connection.sin_family = AF_INET;
		connection.sin_addr.s_addr = inet_addr(dst_addr);

		// send the packet
		sendto(sockfd, packet, ip->tot_len, 0, (struct sockaddr *)&connection, sizeof(struct sockaddr));
		
		//record the start_time
		if(Icmp->icmp_seq == 1)
		{
			gettimeofday(&start_time, NULL);
		}
		
		// listen for responses
		int addrlen = sizeof(connection);
		int ret;
		if (( ret = recvfrom(sockfd, buffer, PACKAGE_SIZE, 0, (struct sockaddr *)&connection, &addrlen)) == -1)
		{
			perror("recv error");
		}
		//recv success
		else
		{
			recv_num += 1;
			ip_reply = (struct iphdr*) buffer;
			icmp_reply = (struct icmp*)(buffer + sizeof(struct iphdr));
			
			//Time to live exceeded
			if(icmp_reply->icmp_type == ICMP_TIME_EXCEEDED)
			{
				printf("Time to live exceeded\n");
			}
			//reach the destination
			else
			{
				struct timeval *sendtime, recvtime, out;
				//ICMP_data is the send_time which is set when send
				sendtime = (struct timeval*)(icmp_reply->icmp_data);
				gettimeofday(&recvtime, NULL);
				calc_time_used(&recvtime, sendtime, &out);
				double time_used = out.tv_sec * 1000.0 + out.tv_usec / 1000.0;
				
				printf("%d bytes from %s: icmp_seq=%d ttl=%d RTT=%.1lf ms\n", 
					ret,
					dst_addr,
					icmp_reply->icmp_seq,
					ip_reply->ttl,
					time_used);
			}
		}
		
		//sleep a while each request
		sleep(1);
	}
	
	
	free(packet);
	free(buffer);
	close(sockfd);
	return 0;
}

