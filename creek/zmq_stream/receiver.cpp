//#include "zhelpers.hpp"
#include <iostream>
#include <zmq.hpp>
#include <pthread.h>
#include <time.h>

#include "Source.hpp"

#define MESSAGE_COUNT	500

#define X4 1

struct thread_arg {
	const char *connect_to;
	int32_t tid;
};


static double get_exec_time_ms(struct timespec start, struct timespec stop){
	return (double)((stop.tv_sec - start.tv_sec) * 1000) + (double)((stop.tv_nsec - start.tv_nsec) / 1000000.0);
}

static void *receiver(void *arg) {
	// context_t context(int io_threads);
	// io_threads argument specifies the size of the ØMQ thread pool to handle I/O operations
	struct thread_arg *t_arg = (thread_arg *) arg;
	const char *connect_to = t_arg->connect_to;

	uint64_t nrecs = 5;

	struct timespec t_start, t_stop;
	uint64_t message_size;
	double elapsed;

	zmq::context_t context(1);
	zmq::socket_t receiver(context, ZMQ_PULL);

	/* high water mark setting */
	uint64_t hwm = 100 ;
	/* the defualt ZMQ_HWM value of zero means "no limit". */
#if ZMQ_VERSION < ZMQ_MAKE_VERSION(3, 3, 0)
	receiver.setsockopt(ZMQ_HWM, &hwm, sizeof(hwm));
#else
	sender.setsockopt(ZMQ_SNDHWM, &hwm, sizeof(hwm));
#endif

//	receiver.connect("tcp://localhost:5563");
	receiver.connect(connect_to);

	/* first get message to measure size of message */
	zmq::message_t message;
	receiver.recv(&message);

#ifdef X3
	zmq_source::x3 *bundle;
	bundle = (zmq_source::x3 *) message.data();
#else
	zmq_source::x4 *bundle;
	bundle = (zmq_source::x4 *) message.data();
#endif

	message_size = message.size();

	for (int i = 0; i < 100; i ++) {
		clock_gettime(CLOCK_MONOTONIC, &t_start);
		for (int j = 0; j < MESSAGE_COUNT; j++) {
			//		while (1) {
			//			zmq::message_t message;
			receiver.recv(&message);
#ifdef X3
			bundle = (zmq_source::x3 *) message.data();
#else
			bundle = (zmq_source::x4 *) message.data();
#endif
#if 1
			W("tid : %d, msg_size : %ld", t_arg->tid, message.size());
			for (uint64_t i = 0 ; i < nrecs; i++) {
#ifdef X3
				W("%08ld %08ld %08ld", std::get<0>(bundle[i]), std::get<1>(bundle[i]),
						std::get<2>(bundle[i]));
#else
				W("%08ld %08ld %08ld %08ld", std::get<0>(bundle[i]), std::get<1>(bundle[i]),
						std::get<2>(bundle[i]), std::get<3>(bundle[i]));
#endif
			}
			W("\n");
#endif
		}
		clock_gettime(CLOCK_MONOTONIC, &t_stop);
		elapsed = get_exec_time_ms(t_start, t_stop);

		W("tid : %d", t_arg->tid);
		EE("time elapsed : %.3f [ms]", elapsed);
		I("message size :%ld [Bytes]", message_size);
		I("message count : %d", MESSAGE_COUNT);
		EE("mean throughput : %d [msg/s]", (int) ((double)MESSAGE_COUNT / (elapsed / 1000)));
		I("mean throughput %.3f [Mb/s]", (double) (MESSAGE_COUNT * message_size * 8) / 1000000 / (elapsed / 1000));
		EE("mean throughput %.3f [MB/s]", (double) (MESSAGE_COUNT * message_size) / 1000000 / (elapsed / 1000));
	}
	return NULL;
}


int main (int argc, const char *argv[]) {
	const char *connect_to;
	int32_t thread_num = 1;
	int32_t rc;

	if (argc < 2) {
		EE("need to specify address to connect");
		exit(1);
	}
	connect_to = argv[1];

	if (argc == 3) {
		thread_num = atoi (argv[2]);
	}

	I("connect to : %s", connect_to);
	I("number of threads : %d", thread_num);

	pthread_t threads[thread_num];
	struct thread_arg t_arg[thread_num];

	for (int i = 0; i < thread_num; i++) {
		t_arg[i].connect_to = connect_to;
		t_arg[i].tid = i;

		rc = pthread_create(&threads[i], NULL, receiver, (void *)&t_arg[i]);
		if (rc != 0)
			EE("error in pthread_create : %s\n", zmq_strerror (rc));
	}

	for (int i = 0; i < thread_num; i++) {
		rc = pthread_join(threads[i], NULL);
	}

#if 0
    //  Prepare our context and subscriber
    zmq::context_t context(1);
	zmq::socket_t receiver(context, ZMQ_PULL);
//    receiver.connect("tcp://localhost:5563");
    receiver.connect(connect_to);

//    subscriber.setsockopt( ZMQ_SUBSCRIBE, "B", 1);

	zmq_source::x4 *bundle;

	uint64_t nrecs = 20;
    while (1) {
		zmq::message_t message;
		receiver.recv(&message);

		bundle = (zmq_source::x4 *) message.data();
		for (uint64_t i = 0 ; i < nrecs; i++) {
//			cout << std::get<0>(bundle[i]) << std::get<1>(bundle[i]) << std::get<2>(bundle[i]) << endl;
			cout << std::get<0>(bundle[i]) << std::get<1>(bundle[i]) << std::get<2>(bundle[i]) << std::get<3>(bundle[i]) << endl;
		}

		printf("\n");

//		std::string smessage(static_cast<char*>(message.data()), message.size());
//		std::cout << smessage << std::endl;


		/*
		//  Read envelope with address
		std::string address = s_recv (subscriber);
		//  Read message contents
		std::string contents = s_recv (subscriber);

        std::cout << "[" << address << "] " << contents << std::endl;
		*/
    }
#endif
    return 0;
}
