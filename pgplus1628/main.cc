/*
 * Author : ubuntu
 * Email : yajin160305@gmail.com
 * File : main.cc
 * CreateDate : 2021-11-14 15:28:04
 * */

#include "coordinator.hpp"
#include <pthread.h>
#include <glog/logging.h>

#define NNODE (8)

struct TXArgs {
	Sender * tx;
};

void * tx_fn(void *args_) {
	struct TXArgs * args = static_cast<struct TXArgs*>(args_);
	Sender * tx = args->tx;

	int times = 3;
	for(int i = 0;i < times;i ++) {
		uint64_t val = (uint64_t)(tx->get_id()) + 1;
		for(int rid = 0; rid < NNODE; rid ++) {
			tx->signal(rid, val);
			LOG(INFO) << tx->get_id() << " -> " << rid << " val " << val;
		}
	}

	return NULL;
}


int main(int argc, char **argv) {

	Receiver * rxs[NNODE];
	Sender *txs[NNODE];

	for(int i = 0;i < NNODE; i ++) {
		rxs[i] = new Receiver(NNODE, i);
	}

	for(int i = 0;i < NNODE;i ++) {
		txs[i] = new Sender(NNODE, i, rxs);
	}

	// start listeners
	for(int i = 0;i < NNODE;i ++) {
		rxs[i]->start_listener();
	}


	// start senders
	pthread_t senders[NNODE];
	for(int i = 0;i < NNODE;i ++) {
		struct TXArgs * sarg = (struct TXArgs*) malloc(sizeof(TXArgs));
		sarg->tx = txs[i];
		pthread_create(&(senders[i]), NULL, tx_fn, (void*) sarg);
	}

	// join threads
	for(int i = 0;i < NNODE;i ++) {
		pthread_join(senders[i], NULL);
	}

	for(int i = 0;i < NNODE;i ++) {
		delete rxs[i];
		rxs[i] = nullptr;
	}


	return 0;
}

/* vim: set tabstop=4 set shiftwidth=4 */

