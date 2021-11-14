/*
 * Author : ubuntu
 * Email : yajin160305@gmail.com
 * File : server.cc
 * CreateDate : 2021-11-14 15:22:33
 * */

#include "EpollServer.h"

int main(int argc, char const *argv[])
{
	if(argc != 3)
	{
		printf("usage %s port threadnum\n", argv[0]);
		return -1;
	}
	int port = atoi(argv[1]);
	if(port == 0)
	{
		printf("port must be Integer\n");
		return -1;
	}
	int threadnum = atoi(argv[2]);
	if(port == 0)
	{
		printf("threadnum must be Integer\n");
		return -1;
	}
	EpollServer *epoll = new EpollServer(port, threadnum);

	epoll->init();

	epoll->epoll();
	return 0;
}

/* vim: set tabstop=4 set shiftwidth=4 */

