/*
 * Author : ubuntu
 * Email : yajin160305@gmail.com
 * File : server.cc
 * CreateDate : 2021-11-14 15:12:55
 * */

// server.cpp
#include <iostream>
#include <cstdio>
#include <cstring>
extern "C" {
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <pthread.h>
#include <unistd.h>
}
#define MAX_BACKLOG 10
void *readerthread(void *args)
{
    int epfd = *((int *)args);
    epoll_event outwait[10];
    while (true) {
        int retpw = epoll_wait(epfd, outwait, 20, 3000);
        if (retpw == -1) {
            printf("epoll error %m\n");
        } else if (retpw == 0) {
            printf("nothing is ready yet\n");
            continue;
        } else {
            for (int i = 0; i < retpw; i++) {
                if (outwait[i].events & EPOLLIN) {
                    int fd = outwait[i].data.fd;
                    char buf[64];
                    if (-1 == read(fd, buf, 64)) {
                        printf("error reading %m\n");
                    }
                    printf("%s\n", buf);
                } else {
                    std::cout << "other event" << std::endl;
                }
            }
        }
    }
}

int main()
{

    int epfd = epoll_create(10);
    if (-1 == epfd) {
        std::cerr << "error creating EPOLL server" << std::endl;
        return -1;
    }

    pthread_t reader;
    int rt = pthread_create(&reader, NULL, readerthread, (void *)&epfd);
    if (-1 == rt) {
        printf("thread creation %m\n");
        return -1;
    }

    struct addrinfo addr;
    memset(&addr, 0, sizeof(addrinfo));
    addr.ai_family = AF_INET;
    addr.ai_socktype = SOCK_STREAM;
    addr.ai_protocol = 0;
    addr.ai_flags = AI_PASSIVE;

    struct addrinfo *rp, *result;
    getaddrinfo("localhost", "59000", &addr, &result);
    for (rp = result; rp != NULL; rp = rp->ai_next) {

        // we want to take the first ( it could be IP_V4
        // or IP_V6 )
        break;
    }

    int sd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sd) {
        std::cerr << "error creating the socket" << std::endl;
        return -1;
    }
    // to avoid error 'Address already in Use'
    int optval = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (-1 == bind(sd, result->ai_addr, result->ai_addrlen)) {
        printf("%m\n");
        std::cerr << "error binding" << std::endl;
        return -1;
    }

    while (true) {

        std::cout << "listen" << std::endl;
        if (-1 == listen(sd, MAX_BACKLOG)) {
            std::cerr << "listen didn't work" << std::endl;
            return -1;
        }

        std::cout << "accept" << std::endl;
        sockaddr peer;
        socklen_t addr_size;
        int pfd = accept(sd, &peer, &addr_size);
        if (pfd == -1) {
            std::cerr << "error calling accept()" << std::endl;
            return -1;
        }
        epoll_event ev;
        ev.data.fd = pfd;
        ev.events = EPOLLIN;
        std::cout << "adding to epoll list" << std::endl;
        if (-1 == epoll_ctl(epfd, EPOLL_CTL_ADD, pfd, &ev)) {
            printf("epoll_ctl error %m\n");
            return -1;
        }

    }

}


/* vim: set tabstop=4 set shiftwidth=4 */

