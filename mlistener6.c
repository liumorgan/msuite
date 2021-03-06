/**
 * mlistener6.c  
 *  joins a multicast group and echoes all data it receives 
 *
 * dropletzhu@gmail.com
 *
 * ChangeLog
 *	- 2009-09-27
 *		- copy from mlistener.c and revise for ipv6
 */

#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <net/if.h>

#define version "0.1"
#define IP_LEN 40
#define PORT_LEN 6
#define NAME_LEN 10
#define BUF_LEN 65535

unsigned int l_count = 0;
/* map sender source ip's least two bytes to this array
 * one to one mapping, do not check conflict
 */
unsigned int s_count[65535];

void usage()
{
	printf("Usage: ./mlistener -s source -i interface -g group -p port -l length \n");
	printf(" -s souurce	use the source to select bind device\n");
	printf(" -i ifname	interface name\n");
	printf(" -g group	the group to listen\n");
	printf(" -p port	the port to listen\n");
	printf(" -l length	the packet length, default length is 256 bytes\n");
	printf(" Version 	%s\n", version);
}

void term_handler(int signum)
{
	unsigned int s_total, i;

	for( i = 0; i < 65535; i++ ) {
		s_total += s_count[i];
	}

	printf("Total received:	%u\n", l_count);
	printf("Total sent:	%u\n", s_total);
	/* the s_count may not be updated when interrupt the
	   process */
	if( l_count <= s_total )
		printf("Total lost:	%u\n", s_total - l_count);
	else
		printf("Total lost: 0\n");

	exit(0);
}

int
main (int argc, char *argv[])
{
	struct sockaddr_in6 addr;
	int fd, nbytes, ch, i;
	unsigned int addrlen;
	struct ipv6_mreq mreq;
	char msgbuf[BUF_LEN], source[IP_LEN], group[IP_LEN], port[PORT_LEN];
	char ifname[NAME_LEN];
	unsigned int ifindex;
	int length = 256;
	struct sigaction new_action, old_action;
	char s_source[IP_LEN], s_group[IP_LEN];

	if (argc <= 1)
	{
		usage();
		return 0;
	}

	while ((ch = getopt (argc, argv, "s:i:g:p:l:")) != -1)
	{
		switch (ch)
		{
		case 's':
			memset (source, 0, IP_LEN);
			strncpy (source, optarg, IP_LEN - 1);
			break;
		case 'i':
			memset (ifname, 0, NAME_LEN);
			strncpy (ifname, optarg, NAME_LEN - 1);
			break;
		case 'g':
			memset (group, 0, IP_LEN);
			strncpy (group, optarg, IP_LEN - 1);
			break;
		case 'p':
			memset (port, 0, PORT_LEN);
			strncpy (port, optarg, PORT_LEN - 1);
			break;
		case 'l':
			length = atoi(optarg);
			break;
		default:
			usage();
			return 0;
		}
	}

	/* create what looks like an ordinary UDP socket */
	if ((fd = socket (AF_INET6, SOCK_DGRAM, 0)) < 0)
	{
		perror ("socket");
		return -1;
	}

	/* set up destination address */
	memset (&addr, 0, sizeof (addr));
	addr.sin6_family = AF_INET6;
	inet_pton(AF_INET6, group, &(addr.sin6_addr));
	addr.sin6_port = htons (atoi (port));

	/* bind to receive address */
	if (bind (fd, (struct sockaddr *) &addr, sizeof (addr)) < 0)
	{
		perror ("bind");
		return -1;
	}

	/* use setsockopt() to request that the kernel join a multicast group */
	inet_pton(AF_INET6, group, &(mreq.ipv6mr_multiaddr));
	ifindex = if_nametoindex(ifname);
	mreq.ipv6mr_interface = ifindex;

	if (setsockopt (fd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq, sizeof (mreq)) < 0)
	{
		perror ("setsockopt add");
		return -1;
	}

	new_action.sa_handler = term_handler;
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = 0;
	sigaction(SIGINT, &new_action, &old_action);
	sigaction(SIGKILL, &new_action, &old_action);
	sigaction(SIGTERM, &new_action, &old_action);

	while (1)
	{
		memset(msgbuf, 0, BUF_LEN);

		addrlen = sizeof (addr);
		if ((nbytes = recvfrom (fd, msgbuf, length, 0, (struct sockaddr *) &addr, &addrlen)) < 0)
		{
			perror ("recvfrom");
			return -1;
		}
		printf ("Receiver %d -- %s\n", ++l_count, msgbuf);
		sscanf (msgbuf,"Sender %[0-9.]->%[0-9.] <> %d",s_source,s_group,&i);
		s_count[atoi(s_source) & 0xFF] = i;
	}
}
