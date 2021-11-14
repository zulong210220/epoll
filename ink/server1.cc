/*
 * Author : ubuntu
 * Email : yajin160305@gmail.com
 * File : server1.cc
 * CreateDate : 2021-11-14 15:45:22
 * */

    #include <sys/socket.h>
    #include <sys/epoll.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <stdio.h>
    #include <errno.h>
    #include <stdlib.h>
    #include <string.h>



    #define MAXLINE 5
    #define OPEN_MAX 100
    #define LISTENQ 20
    #define SERV_PORT 5000
    #define INFTIM 1000

    void setnonblocking(int sock)
    {
        int opts;
        opts=fcntl(sock,F_GETFL);
        if(opts<0)
        {
            perror("fcntl(sock,GETFL)");
            exit(1);
        }
        opts = opts|O_NONBLOCK;
        if(fcntl(sock,F_SETFL,opts)<0)
        {
            perror("fcntl(sock,SETFL,opts)");
            exit(1);
        }
    }

    int main(int argc, char* argv[])
    {
        int i, maxi, listenfd, connfd, sockfd,epfd,nfds, portnumber;
        ssize_t n;
        char line[MAXLINE];
        socklen_t clilen;


        if ( 2 == argc )
        {
            if( (portnumber = atoi(argv[1])) < 0 )
            {
                fprintf(stderr,"Usage:%s portnumber/a/n",argv[0]);
                return 1;
            }
        }
        else
        {
            fprintf(stderr,"Usage:%s portnumber/a/n",argv[0]);
            return 1;
        }



        //Declare variables for the epoll_event structure, ev for registering events, and array for returning events to process

        struct epoll_event ev,events[20];
        //Generate epoll-specific file descriptors for processing accept s

        epfd=epoll_create(256);
        struct sockaddr_in clientaddr;
        struct sockaddr_in serveraddr;
        listenfd = socket(AF_INET, SOCK_STREAM, 0);
        //Set socket to non-blocking

        //setnonblocking(listenfd);

        //Set the file descriptor associated with the event to be processed

        ev.data.fd=listenfd;
        //Set the type of event to process

        ev.events=EPOLLIN|EPOLLET;
        //ev.events=EPOLLIN;

        //Register epoll events

        epoll_ctl(epfd,EPOLL_CTL_ADD,listenfd,&ev);
        bzero(&serveraddr, sizeof(serveraddr));
        serveraddr.sin_family = AF_INET;
        char *local_addr="127.0.0.1";
        inet_aton(local_addr,&(serveraddr.sin_addr));//htons(portnumber);

        serveraddr.sin_port=htons(portnumber);
        bind(listenfd,(struct sockaddr *)&serveraddr, sizeof(serveraddr));
        listen(listenfd, LISTENQ);
        maxi = 0;
        for ( ; ; ) {
            //Waiting for the epoll event to occur

            nfds=epoll_wait(epfd,events,20,500);
            //Handle all events that occur

            for(i=0;i<nfds;++i)
            {
                if(events[i].data.fd==listenfd)//If a new SOCKET user is detected to be connected to a bound SOCKET port, establish a new connection.

                {
                    connfd = accept(listenfd,(struct sockaddr *)&clientaddr, &clilen);
                    if(connfd<0){
                        perror("connfd<0");
                        exit(1);
                    }
                    //setnonblocking(connfd);

                    char *str = inet_ntoa(clientaddr.sin_addr);
                    printf("accapt a connection from\n ");
                    //Setting file descriptors for read operations

                    ev.data.fd=connfd;
                    //Set Read Action Events for Annotation

                    ev.events=EPOLLIN|EPOLLET;
                    //ev.events=EPOLLIN;

                    //Register ev

                    epoll_ctl(epfd,EPOLL_CTL_ADD,connfd,&ev);
                }
                else if(events[i].events&EPOLLIN)//If the user is already connected and receives data, read in.

                {
                    printf("EPOLLIN\n");
                    if ( (sockfd = events[i].data.fd) < 0)
                        continue;
                    if ( (n = read(sockfd, line, MAXLINE)) < 0) {
                        if (errno == ECONNRESET) {
                            close(sockfd);
                            events[i].data.fd = -1;
                        } else
                            printf("readline error\n");
                    } else if (n == 0) {
                        close(sockfd);
                        events[i].data.fd = -1;
                    }
                    if(n<MAXLINE-2)
                        line[n] = '\0';

                    //Setting file descriptors for write operations

                    ev.data.fd=sockfd;
                    //Set Write Action Events for Annotation

                    ev.events=EPOLLOUT|EPOLLET;
                    //Modify the event to be handled on sockfd to EPOLLOUT

                    //epoll_ctl(epfd,EPOLL_CTL_MOD,sockfd,&ev);

                }
                else if(events[i].events&EPOLLOUT) // If there is data to send

                {
                    sockfd = events[i].data.fd;
                    write(sockfd, line, n);
                    //Setting file descriptors for read operations

                    ev.data.fd=sockfd;
                    //Set Read Action Events for Annotation

                    ev.events=EPOLLIN|EPOLLET;
                    //Modify the event to be processed on sockfd to EPOLIN

                    epoll_ctl(epfd,EPOLL_CTL_MOD,sockfd,&ev);
                }
            }
        }
        return 0;
    }



/* vim: set tabstop=4 set shiftwidth=4 */

