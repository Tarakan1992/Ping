#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netdb.h>
#include <sys/time.h>
#include <errno.h>
#define h_addr h_addr_list[0]
#define BUFSIZE 100

struct sockaddr_in servaddr;
int sd;
int pid;
int nsent;
int nreceived;
int tsum = 0;
int tmin = 1000;
int tmax = 0;

void catcher(int sig);
void pinger();
void output(char *ptr, int len, struct timeval *tvrecv);
unsigned short in_cksum(unsigned short *addr, int len);
void tv_sub(struct timeval *tv2, struct timeval *tv1);

int main(int argc, char* argv[])
{	
	if(argc < 2)
	{
		perror("\nUsage: ping [ip address or hostname].\n");
		exit(-1);
	}

	nsent = 0;
	nreceived = 0;

	struct hostent* hp;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	
	if(inet_addr(argv[1]) != INADDR_NONE)
	{
		servaddr.sin_addr.s_addr = inet_addr(argv[1]);
	}
	else
	{
		if(hp = gethostbyname(argv[1]))
		{
			servaddr.sin_addr= *((struct in_addr *)hp->h_addr);
		}
		else
		{
			perror("\nIncorrect ip-address or hostname.\n");
			exit(-1);
		}
	}

	pid = (int)getpid();

	struct sigaction act;

	memset(&act, 0, sizeof(act));

 	act.sa_handler = &catcher;
	sigaction(SIGALRM, &act, NULL);
	sigaction(SIGINT, &act, NULL);


	sd = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP);

	if(sd < 0)
	{
	 	perror("socket error.\n");
	 	exit(-1);
	}

	int size = 60 * 1024;

	setsockopt(sd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));


	//timer initializing
	struct itimerval timer;
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 1;
	timer.it_interval.tv_sec = 1;
	timer.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &timer, NULL);

	int n; 
	struct timeval tval;
	int servlen = sizeof(servlen);
	char recvbuf[BUFSIZE];

	while(1)
	{
		n = recvfrom(sd, recvbuf, BUFSIZE, 0, 
			(struct sockaddr*)&servaddr, &servlen);

		if(n < 0)
		{
			if(errno == EINTR)
			{
				continue;
			}
			perror("recvfrom failed");
			continue;
		}

		gettimeofday(&tval, NULL);
		output(recvbuf, n, &tval);
	}

	close(sd);
	return 1;

}

void catcher(int sig)
{
	if (sig == SIGALRM)
 	{
 		pinger(); 
 		return;
 	} else if (sig == SIGINT) {
 		exit(-1);
 	}
}

void pinger()
{
	int icmplen;
	struct icmp *icmp;
	char sendbuf[BUFSIZE];
	

	icmp = (struct icmp *) sendbuf;

	icmp->icmp_type = ICMP_ECHO;
	icmp->icmp_code = 0;
	icmp->icmp_id = pid;
	icmp->icmp_seq = nsent++;
	gettimeofday((struct timeval *) icmp->icmp_data, NULL);
	icmplen = 8 + 56;
	icmp->icmp_cksum = 0;
	icmp->icmp_cksum = in_cksum((unsigned short*)icmp, icmplen);

	if(sendto(sd, sendbuf, icmplen, 0,
	 (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
	{
		perror("sendto failed");
		exit(-1);
	}
}

void output(char *ptr, int len, struct timeval *tvrecv)
{
	int iplen;
	int icmplen;
	struct ip *ip;
	struct icmp *icmp;
	struct timeval *tvsend;
	double rtt;

	ip = (struct ip *) ptr;
	iplen = ip->ip_hl << 2;

	icmp = (struct icmp *) (ptr + iplen);

	if((icmplen = len - iplen) < 8)
	{
		fprintf(stderr, "icmplen (%d) < 8", icmplen);
	}

	if(icmp->icmp_type == ICMP_ECHOREPLY)
	{
		if(icmp->icmp_id != pid)
		{
			return;
		}

		tvsend = (struct timeval*) icmp->icmp_data;
		tv_sub(tvrecv, tvsend);

		rtt = tvrecv->tv_sec * 1000.0 + tvrecv->tv_usec / 1000.0;

		nreceived++;

		tsum += rtt;
		if(rtt < tmin)
		{
			tmin = rtt;
		}

		if(rtt > tmax)
		{
			tmax = rtt;
		}

		printf("%d bytes from %s: icmp_seq=%u, ttl=%d, time=%.3f ms\n",
			icmplen, inet_ntoa(servaddr.sin_addr),
			icmp->icmp_seq, ip->ip_ttl, rtt);
	}
}


unsigned short in_cksum(unsigned short *addr, int len) 
{
	unsigned short result;
 	unsigned int sum = 0;
	
	while (len > 1) 
	{
 		sum += *addr++;
 		len -= 2;
 	}
 	
 	if (len == 1)
 	sum += *(unsigned char*) addr;
	
	sum = (sum >> 16) + (sum & 0xFFFF);
 	sum += (sum >> 16);

 	result = ~sum; 
 	
 	return result;
}

void tv_sub(struct timeval *tv2, struct timeval *tv1) 
{
	tv2->tv_sec -= tv1->tv_sec;
	tv2->tv_usec -= tv1->tv_usec;
}