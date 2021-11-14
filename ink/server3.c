/*
 * Author : ubuntu
 * Email : yajin160305@gmail.com
 * File : server3.c
 * CreateDate : 2021-11-14 15:47:43
 * */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <stdbool.h>

#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 10

struct fds
{
    int epollfd;
    int sockfd;
};

int SetNonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void AddFd(int epollfd, int fd, bool oneshot)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    if(oneshot)
    {
        event.events |= EPOLLONESHOT;
    }
    
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    SetNonblocking(fd);
}

/*Reset the event on fd. After this, although the EPOLLONESHOT event on FD is registered, the operating system still triggers the EPOLLIN event on FD and only once*/
void reset_oneshot(int epollfd, int fd)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

/*Work Threads*/
void* worker(void* arg)
{
    int sockfd = ((struct fds*)arg)->sockfd;
    int epollfd = ((struct fds*)arg)->epollfd;
    printf("start new thread to receive data on fd: %d\n", sockfd);
    char buf[BUFFER_SIZE];
    memset(buf, 0, BUFFER_SIZE);
    
    while(1)
    {
        int ret = recv(sockfd, buf,BUFFER_SIZE-1, 0);
        if(ret == 0)
        {
            close(sockfd);
            printf("foreigner closed the connection\n");
            break;
        }
        else if(ret < 0)
        {
            if(errno = EAGAIN)
            {
                reset_oneshot(epollfd, sockfd);
                printf("read later\n");
                break;
            }
        }
        else
        {
            printf("get content: %s\n", buf);
            //Hibernate for 5 seconds to simulate data processing
            printf("worker working...\n");
            sleep(5);
        }
    }
    printf("end thread receiving data on fd: %d\n", sockfd);
}

int main(int argc, char* argv[])
{
    if(argc <= 2)
    {
        printf("usage: ip_address + port_number\n");
        return -1;
    }
    
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    
    int ret = -1;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);
    
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    if(listenfd < 0)
    {
        printf("fail to create socket!\n");
        return -1;
    }
    
    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    if(ret == -1)
    {
        printf("fail to bind socket!\n");
        return -1;
    }
    
    ret = listen(listenfd, 5);
    if(ret == -1)
    {
        printf("fail to listen socket\n");
        return -1;
    }
    
    struct epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    if(epollfd == -1)
    {
        printf("fail to create epoll\n");
        return -1;
    }
    
    //Note that EPOLLONESHOT events cannot be registered on socket listenfd, otherwise the application can only process one client connection!Because subsequent client connection requests will no longer trigger the EPOLLIN event for listenfd
    AddFd(epollfd, listenfd, false);
    
    
    while(1)
    {
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);  //Permanent Wait
        if(ret < 0)
        {
            printf("epoll failure!\n");
            break;
        }
        
        int i;
        for(i = 0; i < ret; i++)
        {
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
                //Register EPOLLONESHOT events for each non-listening file descriptor
                AddFd(epollfd, connfd, true);
            }
            else if(events[i].events & EPOLLIN)
            {
                pthread_t thread;
                struct fds fds_for_new_worker;
                fds_for_new_worker.epollfd = epollfd;
                fds_for_new_worker.sockfd = events[i].data.fd;
                /*Start a new worker thread to serve sockfd*/
                pthread_create(&thread, NULL, worker, &fds_for_new_worker);
                
            }
            else
            {
                printf("something unexpected happened!\n");
            }
        }
    }
    
    close(listenfd);
    
    return 0;
}

/* vim: set tabstop=4 set shiftwidth=4 */

