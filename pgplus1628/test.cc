/*
 * Author : ubuntu
 * Email : yajin160305@gmail.com
 * File : test.cc
 * CreateDate : 2021-11-14 15:28:34
 * */

#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <glog/logging.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>

#define NWRITER (5)


struct write_thr_args {
	int efd;
	uint64_t  w_val;
};

struct read_thr_args {
	int efd;
};

using WArgs = struct write_thr_args;
using RArgs = struct read_thr_args;


void *write_fn(void *thr_args) {
	WArgs * wargs = static_cast<WArgs*>(thr_args);
	int efd = wargs->efd;
	uint64_t counter = wargs->w_val;


	// write to fd
	int ret;
	for(int i = 0;i < 5;i ++) {
		//sleep(1);
		ret = write(efd, &counter, sizeof(counter));
		CHECK_GE(ret, 0) << " write event fd failed.";
		LOG(INFO) << "WRITTER:: write with size " << ret << " val " << counter;
	}

	LOG(INFO) << "WRITTER:: exit.";
	return NULL;
}


void *read_fn(void *thr_args) {
	WArgs * wargs = static_cast<WArgs*>(thr_args);
	int efd = wargs->efd;

	int ret;

	// epoll wait
	int ep_fd = -1;
	struct epoll_event events[10];
	CHECK_GE(efd, 0) << " Invalid efd " << efd;
	ep_fd = epoll_create(1024);
	CHECK_GE(ep_fd, 0) << "epoll_create failed.";

	{
		struct epoll_event read_event;
		read_event.events = EPOLLHUP | EPOLLERR | EPOLLIN;
		read_event.data.fd = efd;

		ret = epoll_ctl(ep_fd, EPOLL_CTL_ADD, efd, &read_event);
		CHECK_GE(ret, 0) << "epoll_ctl failed.";
	}

	uint64_t  counter;

	while(1){
		ret = epoll_wait(ep_fd, &events[0], 10, 5000);
		bool jmp = false;
		if (ret > 0) {
			int i = 0;
			for( ; i < ret; i ++) {
				if (events[i].events & EPOLLHUP) {
					LOG(FATAL) << "epoll eventfd has epoll hup.";
					jmp = true;
				} else if (events[i].events & EPOLLERR) {
					LOG(FATAL) << "epoll eventfd has epoll error.";
					jmp = true;
				} else if (events[i].events & EPOLLIN) {
					int event_fd = events[i].data.fd;
					ret = read(event_fd, &counter, sizeof(counter));
					CHECK_GE(ret, 0) << " read event_fd failed.";
					LOG(INFO) << "READER::read event_fd with size " << ret << " val " << counter;
				}
			}
			if (jmp) break;
		} else if (ret == 0) {
			LOG(INFO) << " epoll wait time out.";
		} else {
			LOG(ERROR) << " epoll wait error : ";
		}

	}

	LOG(INFO) << "READER:: exit.";

	return NULL;

}


int main(int argc, char **argv) {

	int efd = eventfd(0, EFD_NONBLOCK);

	pthread_t writers [NWRITER];
	pthread_t reader;

	int ret = 0;


	// NOTICE : Writter MUST start before Reader
	for(int i = 0;i < NWRITER;i ++) {
		WArgs *wargs = (WArgs*)malloc(sizeof(WArgs)); // TODO free wargs
		wargs->efd = efd;
		wargs->w_val = static_cast<uint64_t>(i) + 1;
		pthread_create(&(writers[i]), NULL, write_fn, (void*)(wargs));
	}


	RArgs * rargs = (RArgs*)malloc(sizeof(RArgs)); // TODO free rargs
	rargs->efd = efd;
	pthread_create(&reader, NULL, read_fn, (void*)(rargs));


	for(int i = 0;i < NWRITER;i ++)
		pthread_join(writers[i], NULL);

	pthread_join(reader, NULL);


	// close efd
	if (efd >= 0) {
		close(efd);
		efd = -1;
	}

	return 0;

}

/* vim: set tabstop=4 set shiftwidth=4 */

