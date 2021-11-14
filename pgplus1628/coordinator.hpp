#pragma once

#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <glog/logging.h>
#include <errno.h>
#include <stdint.h>


#define EPOLL_WAIT_TIMEOUT (1000) // 1.0 second

class Receiver;

struct RXArgs {
	Receiver * rx;
};


class Receiver {
private :
	int kclient_;
	int id_;

public :
	int * chan_fds; // event fds of this channel
	pthread_t listener;

	Receiver(int kclients, int id) : kclient_(kclients), id_(id) {
		chan_fds = new int [kclient_];
		for(int i = 0;i < kclient_;i ++) {
            // ----
			chan_fds[i] = eventfd(0, EFD_NONBLOCK);
			CHECK_NE(chan_fds[i], -1) << " Create eventfd for channel " << i << " failed.";
		}
	}

	// TODO
	void register_read_handler() { }

	void listen() {
		int ret;

		// create epoll event
		struct epoll_event events[kclient_];
		int ep_fd = epoll_create(kclient_);
		CHECK_GE(ep_fd, 0) << "RX::epoll_create failed.";

		for(int i = 0;i < kclient_;i ++) {
			struct epoll_event read_event;
			int efd = chan_fds[i];
			read_event.events = EPOLLHUP | EPOLLERR | EPOLLIN | EPOLLET;
			read_event.data.u32 = i;
			ret = epoll_ctl(ep_fd, EPOLL_CTL_ADD, efd, &read_event);
			CHECK_GE(ret, 0) << "RX " << id_ << " ::epoll_ctl failed.";
		}

		LOG(INFO) << "RX " << id_ << " ::begin listening.";
		// while loop
		uint64_t val;
		while(1) {
			bool ok = true;
			int nfds = epoll_wait(ep_fd, &events[0], kclient_, EPOLL_WAIT_TIMEOUT);
			if (nfds > 0) {
				// handle each ready fd.
				for(int i = 0;i < nfds;i ++) {
					int cid = static_cast<int>(events[i].data.u32);
					if ( events[i].events & EPOLLHUP) {
						LOG(ERROR) << "RX " << id_<< " ::epoll eventfd has epoll hup for " << cid;
						ok = false;
					} else if (events[i].events & EPOLLERR) {
						LOG(ERROR) << "RX " << id_ << " ::epoll eventfd has epoll error for " << cid;
						ok = false;
					} else if (events[i].events & EPOLLIN) {
						int event_fd = chan_fds[cid];
						ret = read(event_fd, &val, sizeof(val));
						CHECK_GE(ret, 0) << "RX" << id_ << " ::read event_fd failed.";
						// TODO read handler
						LOG(INFO) << "RX" << id_ << " ::read from " << cid << " val " << val << " with size " << ret << " bytes.";
					}
					if (!ok) { break; }
				}
				if (!ok) { break; }
			} else if (nfds == 0) {
				LOG(INFO) << "RX " << id_ << " ::epoll wait time out.";
			} else {
				LOG(ERROR) << "RX " << id_ << " ::epoll wait error.";
			}
		}

		LOG(INFO) << "RX " << id_ << " ::exit listen.";
	}

	static void * listen_fn(void * args_) {
		struct RXArgs * args = static_cast<struct RXArgs*>(args_);
		Receiver * rx = args->rx;
		rx->listen();
		free(args);
		pthread_exit(NULL);
	}


	void start_listener() {
		struct RXArgs * args = (struct RXArgs*)malloc(sizeof(struct RXArgs));
		args->rx = this;
		pthread_create(&listener, NULL, Receiver::listen_fn, (void*)args);
	}

	int get_id() { return id_; }

	~Receiver() {
		// join listener
		pthread_join(listener, NULL);
		// close fds
		for(int i = 0;i < kclient_; i ++) {
			if(chan_fds[i] >= 0) { close(chan_fds[i]); chan_fds[i] = -1;}
		}
		// free
		delete chan_fds;
	}


};



class Sender {
private :
	int kserver_;
	int id_;

public :
	int * chan_fds; // channel eventfds, get from Receiver

	Sender(int kserver, int id, Receiver ** servers) : kserver_(kserver), id_(id){
		chan_fds = new int [kserver_];
		for(int i = 0;i < kserver;i ++) {
			chan_fds[i] = servers[i]->chan_fds[id_];
		}
	}

	void signal(int serv_id, uint64_t val) {
		ssize_t ret = 0;
		do {
			ret = write(chan_fds[serv_id], &val, sizeof(val));
		} while(ret < 0 && errno == EAGAIN);
		CHECK_GE(ret, 0) << "TX " << id_ << " -> " << serv_id << " write fd error : " << strerror(errno);
	}

	int get_id() { return id_; }

};
