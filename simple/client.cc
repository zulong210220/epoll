/*
 * Author : ubuntu
 * Email : yajin160305@gmail.com
 * File : client.cc
 * CreateDate : 2021-11-14 15:14:47
 * */

//client.cpp
#include <iostream>
#include <cstring>
#include <cstdio>
extern "C" {
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
}

int main()
{

    const char *servername = "localhost";
    const char *serverport = "59000";

    struct addrinfo server_address;
    memset(&server_address, 0, sizeof(struct addrinfo));
    server_address.ai_family = AF_INET;
    server_address.ai_socktype = SOCK_STREAM;
    server_address.ai_protocol = 0;    // any protocol
    server_address.ai_flags = 0;

    struct addrinfo *result, *rp;

    int res = getaddrinfo(servername, serverport, &server_address, &result);
    if (-1 == res) {
        std::cout << "I cannot getaddress " << servername << std::endl;
        return -1;
    }

    int fd = socket(server_address.ai_family, server_address.ai_socktype, server_address.ai_protocol);
    if (-1 == fd) {
        printf("I cannot open a socket %m\n");
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        std::cout << "************" << std::endl;
        if (-1 == connect(fd, rp->ai_addr, rp->ai_addrlen)) {
            close(fd);
        } else {
            std::cout << "connected" << std::endl;
            break;
        }
    }
    if (rp == NULL) {
        std::cerr << "I couldn't connect server " << servername << std::endl;
    }
    while (true) {
        sleep(2);
        pid_t me = getpid();
        char buf[64];
        bzero(buf, sizeof(buf));
        sprintf(buf, "%ld", me);
        write(fd, buf, sizeof(buf));
        printf("%s\n", buf);
    }
}


/* vim: set tabstop=4 set shiftwidth=4 */

