#ifndef _EPOLL_SERVER_H_
#define _EPOLL_SERVER_H_

#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/epoll.h>
//#include <pthread.h>

#include "thread_pool.h"

#define MAX_EVENT 1024   //epoll_events的最大个数
#define MAX_BUFFER 2048  //Buffer的最大字节

class BaseTask
{
public:
	virtual void doit() = 0;
};

class Task : public BaseTask
{
private:
	int sockfd;
	char order[MAX_BUFFER];
public:
	Task(char *str, int fd) : sockfd(fd)
	{
		memset(order, '\0', MAX_BUFFER);
		strcpy(order, str);
	}
	void doit()  //任务的执行函数
	{
		//do something of the order
		//printf("%s\n", order);
		snprintf(order, MAX_BUFFER - 1, "somedata\n");
		write(sockfd, order, strlen(order));
	}
};

class EpollServer
{
private:
	bool is_stop;   //是否停止epoll_wait的标志
	int threadnum;   //线程数目
	int sockfd;     //监听的fd
	int port;      //端口
	int epollfd;    //Epoll的fd
	threadpool<BaseTask> *pool;   //线程池的指针
	//char address[20];
	epoll_event events[MAX_EVENT];  //epoll的events数组
	struct sockaddr_in bindAddr;   //绑定的sockaddr

public://构造函数
	EpollServer()
	{}
	EpollServer(int ports, int thread) : is_stop(false) , threadnum(thread) ,
		port(ports), pool(NULL)
	{
	}
	~EpollServer()  //析构
	{
		delete pool;
	}

	void init();

	void epoll();

	static int setnonblocking(int fd)  //将fd设置称非阻塞
	{
		int old_option = fcntl(fd, F_GETFL);
		int new_option = old_option | O_NONBLOCK;
		fcntl(fd, F_SETFL, new_option);
		return old_option;
	}

	static void addfd(int epollfd, int sockfd, bool oneshot)  //向Epoll中添加fd
	{//oneshot表示是否设置称同一时刻，只能有一个线程访问fd，数据的读取都在主线程中，所以调用都设置成false
		epoll_event event;
		event.data.fd = sockfd;
		event.events = EPOLLIN | EPOLLET;
		if(oneshot)
		{
			event.events |= EPOLLONESHOT;
		}
		epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event); //添加fd
		EpollServer::setnonblocking(sockfd);
	}

};

void EpollServer::init()   //EpollServer的初始化
{
	bzero(&bindAddr, sizeof(bindAddr));
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_port = htons(port);
	bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        //创建Socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0)
	{
		printf("EpollServer socket init error\n");
		return;
	}
	int ret = bind(sockfd, (struct sockaddr *)&bindAddr, sizeof(bindAddr));
	if(ret < 0)
	{
		printf("EpollServer bind init error\n");
		return;
	}
	ret = listen(sockfd, 10);
	if(ret < 0)
	{
		printf("EpollServer listen init error\n");
		return;
	}
        //create Epoll
	epollfd = epoll_create(1024);
	if(epollfd < 0)
	{
		printf("EpollServer epoll_create init error\n");
		return;
	}
	pool = new threadpool<BaseTask>(threadnum);  //创建线程池
}

void EpollServer::epoll()
{
	pool->start();   //线程池启动
	//
	addfd(epollfd, sockfd, false);
	while(!is_stop)
	{//调用epoll_wait
		int ret = epoll_wait(epollfd, events, MAX_EVENT, -1);
		if(ret < 0)  //出错处理
		{
			printf("epoll_wait error\n");
			break;
		}
		for(int i = 0; i < ret; ++i)
		{
			int fd = events[i].data.fd;
			if(fd == sockfd)  //新的连接到来
			{
				struct sockaddr_in clientAddr;
				socklen_t len = sizeof(clientAddr);
				int confd = accept(sockfd, (struct sockaddr *)
					&clientAddr, &len);

				EpollServer::addfd(epollfd, confd, false);
			}
			else if(events[i].events & EPOLLIN)  //某个fd上有数据可读
			{
				char buffer[MAX_BUFFER];
		readagain:	memset(buffer, 0, sizeof(buffer));
				int ret = read(fd, buffer, MAX_BUFFER - 1);
				if(ret == 0)  //某个fd关闭了连接，从Epoll中删除并关闭fd
				{
					struct epoll_event ev;
					ev.events = EPOLLIN;
					ev.data.fd = fd;
					epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev);
					shutdown(fd, SHUT_RDWR);
					printf("%d logout\n", fd);
					continue;
				}
				else if(ret < 0)//读取出错，尝试再次读取
				{
					if(errno == EAGAIN)	
					{
						printf("read error! read again\n");
						goto readagain;
					    	break;
					}
				}
				else//成功读取，向线程池中添加任务
				{
					BaseTask *task = new Task(buffer, fd);
					pool->append_task(task);
				}
			}
			else
			{
				printf("something else had happened\n");
			}
		}
	}
	close(sockfd);//结束。

	pool->stop();
}

#endif
