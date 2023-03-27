// udp4ward.cpp : Defines the entry point for the console application.
//

#include "netport.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <syslog.h>

#define LIST_TEMP_N  16 
typedef unsigned char u_char;

#define ROLE_CLIENT  0x55
#define ROLE_SERVER  0xaa

struct udpiocb 
{
	int fd;
	int flag;
	time_t last_active;
	struct sockaddr_in addr;
};

int role;
int listen_fd = -1;
struct sockaddr_in remote_addr;
struct udpiocb list_temp[LIST_TEMP_N];

int udpio_same(int id, const struct sockaddr_in *pSrc)
{	
	u_short port = list_temp[id].addr.sin_port;
	u_long  addr = list_temp[id].addr.sin_addr.s_addr;

	if(pSrc->sin_port == port)
		if(pSrc->sin_addr.s_addr == addr)
			return 1;

	return 0;
}

void udpio_list(u_short lport, u_long dest_addr, u_short dport)
{
	int i, error, s_udp;
	struct sockaddr_in addr_in;


	s_udp = socket(PF_INET, SOCK_DGRAM, 0);
	assert(s_udp != -1);

	addr_in.sin_family = AF_INET;
	addr_in.sin_port = htons(lport);
	addr_in.sin_addr.s_addr = htonl(INADDR_ANY);

	error = bind(s_udp, (struct sockaddr *)&addr_in, sizeof(struct sockaddr_in));
	assert(error != -1);
	listen_fd = s_udp;

	remote_addr.sin_family = AF_INET;
	remote_addr.sin_port = htons(dport);
	remote_addr.sin_addr.s_addr = dest_addr;

	for(i = 0; i < LIST_TEMP_N; i++)
	{
		list_temp[i].flag = 0;
		list_temp[i].last_active = 0;
	}
}

void udpio_final(void)
{
	int i;
	for(i = 0; i < LIST_TEMP_N; i++)
	{
		//syslog(LOG_DEBUG, "final: id=%d, fd=%d, active=%ld, addr=%08x, port=%d\n", i, list_temp[i].fd, \
				list_temp[i].last_active, ntohl(list_temp[i].addr.sin_addr.s_addr), \
				ntohs(list_temp[i].addr.sin_port));

		if(list_temp[i].flag)
		{
			list_temp[i].flag = 0;
			list_temp[i].last_active = 0;

			S_CLOSE(list_temp[i].fd);
			list_temp[i].fd = -1;
		}
	}

	S_CLOSE(listen_fd);
	listen_fd = -1;

	syslog(LOG_NOTICE, "Final Done\n");
}

int udpio_collect()
{
	int i;
	int old_id = -1;
	time_t oldest = 0;

	for(i = 0; i < LIST_TEMP_N; i++)
	{
		if(list_temp[i].flag)
		{
			if((old_id == -1) || (list_temp[i].last_active < oldest))
			{
				old_id = i;
				oldest = list_temp[i].last_active;
			}
		}
	}

	S_CLOSE(list_temp[old_id].fd);
	list_temp[old_id].fd = -1;

	list_temp[old_id].flag = 0;
	list_temp[old_id].last_active = 0;

	return old_id;
}

int udpio_realloc(const struct sockaddr_in *pSrc)
{
	int hole = -1;
	int i, error, s_udp;

	for(i = 0; i < LIST_TEMP_N; i++)
	{
		if(list_temp[i].flag)
		{
			if(udpio_same(i, pSrc))
			{
				time(&list_temp[i].last_active);
				//syslog(LOG_DEBUG, "realloc update: id=%d, fd=%d, active=%ld, addr=%08x, port=%d\n", \
					i, list_temp[i].fd, list_temp[i].last_active, \
					ntohl(list_temp[i].addr.sin_addr.s_addr), ntohs(list_temp[i].addr.sin_port));

				return list_temp[i].fd;
			}
		}
		else
		{
			if(hole == -1)
				hole = i;
		}
	}

	if(hole == -1)
	{
		//启动回收
		syslog(LOG_WARNING, "List Overflow!!\n");
		hole = udpio_collect();
	}

	//get a new port
	s_udp = socket(PF_INET, SOCK_DGRAM, 0);
	assert(s_udp != -1);
	
	error = connect(s_udp, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr_in));
	assert(error != -1);

	list_temp[hole].fd = s_udp;
	time(&list_temp[hole].last_active);

	list_temp[hole].addr.sin_family = AF_INET;
	list_temp[hole].addr.sin_port = pSrc->sin_port;
	list_temp[hole].addr.sin_addr.s_addr = pSrc->sin_addr.s_addr;

	list_temp[hole].flag = 1;

	//syslog(LOG_DEBUG, "realloc new: id=%d, fd=%d, active=%ld, addr=%08x, port=%d\n", \
			hole, list_temp[hole].fd, list_temp[hole].last_active, \
			ntohl(list_temp[hole].addr.sin_addr.s_addr), ntohs(list_temp[hole].addr.sin_port));

	return s_udp;
}

u_char table_l[256];
u_char table_r[256];

u_char udpio_byte_swap(u_char src)
{
	u_char dst = 0;
	u_char hig = 0;
	u_char low = 0;

	hig = (src & 0xf0) >> 4;
	low = (src & 0x0f) << 4;
	dst = hig | low;

	return dst;
}

u_char udpio_byte_bitwise(u_char src)
{
	u_char dst = src^0xff;
	return dst;
}

void udpio_table()
{
	int i;
	for(i = 0; i < 256; i++)
	{
		table_r[i] = udpio_byte_swap((u_char)i);	
		table_l[i] = udpio_byte_bitwise((u_char)i);
	}
}

void udpio_data(char *buf, size_t len, int step)
{
	size_t i;
	u_char *ptable;

	if(role == 0)
		return; 

	ptable = (step ? table_l : table_r);
	for(i = 0; i < len; i++)
	{
		buf[i] = ptable[(u_char)buf[i]];	
	}
}

int udpio_fd_set(fd_set *pfds)
{
	int i, fd;

	int fd_max = listen_fd;

	for(i = 0; i < LIST_TEMP_N; i++)
	{
		if(list_temp[i].flag)
		{
			fd = list_temp[i].fd;

			FD_SET(fd, pfds);
			fd_max = (fd_max < fd? fd: fd_max);
		}
	}

	return fd_max;
}

void udpio_switch()
{
	int i, fd; 
	char buf[4096];
	size_t rlen = 0;
	size_t wlen = 0;
	socklen_t addr_len;
	struct sockaddr_in addr_in;

	int count, max_fd;
	struct timeval timeout;
	fd_set readfds, writefds, errorfds;

	FD_ZERO(&errorfds);
	FD_ZERO(&writefds);

	while(1)
	{
		FD_ZERO(&readfds);
		FD_SET(listen_fd, &readfds);
		max_fd = udpio_fd_set(&readfds);

		//timeout.tv_sec = 1;
		//timeout.tv_usec = 0;
		count = select(max_fd + 1, &readfds, NULL, NULL, NULL);

		if (count == -1) 
		{
			syslog(LOG_ERR, "select error!\n"); return;
		}

		if(count > 0)
		{
			if(FD_ISSET(listen_fd, &readfds))
			{
				addr_len = sizeof(struct sockaddr_in);
				rlen = recvfrom(listen_fd, buf, sizeof(buf), 0, (struct sockaddr *)&addr_in, &addr_len);

				udpio_data(buf, rlen, 0);
				fd = udpio_realloc((const struct sockaddr_in *)&addr_in);
				wlen = send(fd, buf, rlen, 0);

				//syslog(LOG_DEBUG, "event listen: saddr=%08x, sport=%d, rlen=%d, wlen=%d\n", \
						ntohl(addr_in.sin_addr.s_addr), ntohs(addr_in.sin_port), rlen, wlen);
			}

			for(i = 0; i < LIST_TEMP_N; i++)
			{
				if(list_temp[i].flag)
				{
					if(FD_ISSET(list_temp[i].fd, &readfds))
					{
						rlen = recv(list_temp[i].fd, buf, sizeof(buf), 0);

						udpio_data(buf, rlen, 1);

						//using listened port
						wlen = sendto(listen_fd, buf, rlen, 0, (struct sockaddr *)&list_temp[i].addr, sizeof(struct sockaddr_in));

						//syslog(LOG_DEBUG, "event return: i=%d, rlen=%d, wlen=%d\n", i, rlen, wlen); 
					}
				}
			}
		}
	}
}

void udpio_init(int argc, char *argv[])
{
	int l_port = 0;	
	int d_port = 0;	
	u_long dest_addr = INADDR_NONE;

	if(argc != 5)
		err_quit("argc error\n");

	switch(argv[1][0])
	{
		case 'c':
			role = ROLE_CLIENT;
			break;
		case 's':
			role = ROLE_SERVER;
			break;
		case 'b': //bridge
		default:
			role = 0;
	}

	l_port = atoi(argv[2]);
	if(l_port<=0 || l_port > 65535)
		err_quit("listen port error\n");

	dest_addr = inet_addr(argv[3]);
	if(dest_addr == INADDR_NONE)
		err_quit("destination address error\n");

	d_port = atoi(argv[4]);
	if(d_port<=0 || d_port > 65535)
		err_quit("destination port error\n");

	printf("list: lport==%d, dest_addr=%08x, dport=%d\n", l_port, ntohl(dest_addr), d_port);
	daemonize("udp4ward");

	udpio_table();
	udpio_list((u_short)l_port, dest_addr, (u_short)d_port);
}

/* udp_switch addr1:port1 */
int main(int argc, char *argv[])
{
#ifdef WIN32
	WSADATA data;
	WSAStartup(0x201, &data);
#endif

	udpio_init(argc, argv); 

	udpio_switch();	

	//XXX
	udpio_final();
	
#ifdef WIN32
	WSACleanup();
#endif
	exit(0);
}
