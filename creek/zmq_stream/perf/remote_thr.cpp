/*
    Copyright (c) 2007-2016 Contributors as noted in the AUTHORS file

    This file is part of libzmq, the ZeroMQ core engine in C++.

    libzmq is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License (LGPL) as published
    by the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    As a special exception, the Contributors give you permission to link
    this library with independent modules to produce an executable,
    regardless of the license terms of these independent modules, and to
    copy and distribute the resulting executable under terms of your choice,
    provided that you also meet, for each linked independent module, the
    terms and conditions of the license of that module. An independent
    module is a module which is not derived from or based on this library.
    If you modify this library, you must extend this exception to your
    version of the library.

    libzmq is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
	FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
	License for more details.

	You should have received a copy of the GNU Lesser General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
	*/

#include "../include/zmq.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

const char *connect_to;
static int message_count;
static size_t message_size;

static void *worker (void *tid)
{
	/* hp */
	void *ctx;
	/*********/

	void *s;
	int rc;
	int i;
	zmq_msg_t msg;
	int id = *(int *)tid;

	unsigned long elapsed;
	double throughput;
	double megabits;
//	double latency;

	void *watch;

//	ctx = zmq_init (id);
	ctx = zmq_init (1);
	if (!ctx) {
		printf ("error in zmq_init: %s\n", zmq_strerror (errno));
		exit(1);
	}

	s = zmq_socket (ctx, ZMQ_PULL);
	if (!s) {
		printf ("error in zmq_socket: %s\n", zmq_strerror (errno));
		exit(1);
	}

	rc = zmq_connect (s, connect_to);
	if (rc != 0) {
		printf ("error in zmq_connect: %s\n", zmq_strerror (errno));
		exit(1);
	}

	rc = zmq_msg_init (&msg);
	if (rc != 0) {
		printf ("error in zmq_msg_init: %s\n", zmq_strerror (errno));
		exit(1);
	}

	printf("%d start | msg_count : %d\n", id, message_count);

	rc = zmq_recvmsg (s, &msg, 0);
	if (rc < 0) {
		printf ("error in zmq_recvmsg: %s\n", zmq_strerror (errno));
		exit(1);
	}
	if (zmq_msg_size (&msg) != message_size) {
		printf ("message of incorrect size received\n");
		exit(1);
	}


	for( int j =0; j < 100; j++) {
	watch = zmq_stopwatch_start ();
	for (i = 0; i != message_count - 1; i++) {
#if 0
        rc = zmq_sendmsg (s, &msg, 0);
        if (rc < 0) {
            printf ("error in zmq_sendmsg: %s\n", zmq_strerror (errno));
            exit(1);
        }
#endif
        rc = zmq_recvmsg (s, &msg, 0);
        if (rc < 0) {
            printf ("error in zmq_recvmsg: %s\n", zmq_strerror (errno));
            exit(1);
        }
        if (zmq_msg_size (&msg) != message_size) {
            printf ("message of incorrect size received\n");
            exit(1);
        }
//		if (i > 3000) printf("id %d, i : %d\n",id, i);
    }

	elapsed = zmq_stopwatch_stop (watch);
	if (elapsed == 0)
		elapsed = 1;

	printf("%d end\n", id);

	throughput = 
		((double) message_count / (double) elapsed * 1000000);
	megabits = ((double) throughput * message_size * 8) / 1000000;

	printf ("message size: %d [B]\n", (int) message_size);
	printf ("message count: %d\n", (int) message_count);
	printf ("mean throughput: %d [msg/s]\n", (int) throughput);
	printf ("mean throughput: %.3f [Mb/s]\n", (double) megabits);

	
	}

    rc = zmq_msg_close (&msg);
    if (rc != 0) {
        printf ("error in zmq_msg_close: %s\n", zmq_strerror (errno));
        exit(1);
    }

	printf("%d end\n", id);

	throughput = 
		((double) message_count / (double) elapsed * 1000000);
	megabits = ((double) throughput * message_size * 8) / 1000000;

	printf ("message size: %d [B]\n", (int) message_size);
	printf ("message count: %d\n", (int) message_count);
	printf ("mean throughput: %d [msg/s]\n", (int) throughput);
	printf ("mean throughput: %.3f [Mb/s]\n", (double) megabits);

    rc = zmq_close (s);
    if (rc != 0) {
        printf ("error in zmq_close: %s\n", zmq_strerror (errno));
        exit(1);
    }

    rc = zmq_ctx_term (ctx);
    if (rc != 0) {
        printf ("error in zmq_ctx_term: %s\n", zmq_strerror (errno));
        exit(1);
    }

#if defined ZMQ_HAVE_WINDOWS
	return 0;
#else
	return NULL;
#endif
}


int main (int argc, char *argv [])
{
//	const char *connect_to;
//	int roundtrip_count;
//	size_t message_size;
//	void *ctx;
//	void *s;
	int rc;
//	int i;
//	zmq_msg_t msg;

	const int thread_num = 4;
	pthread_t threads[thread_num];
	int tid[thread_num];
//	voiD *watch;
//	unsigned long elapsed;
//	double throughput;
//	double megabits;
//	double latency;

	if (argc != 4) {
		printf ("usage: remote_lat <connect-to> <message-size> "
				"<roundtrip-count>\n");
		return 1;
	}
	connect_to = argv [1];
	message_size = atoi (argv [2]);
	message_count = atoi (argv [3]);


#if 0
	ctx = zmq_init (1);
	if (!ctx) {
		printf ("error in zmq_init: %s\n", zmq_strerror (errno));
		return -1;
	}

	s = zmq_socket (ctx, ZMQ_REQ);
	if (!s) {
		printf ("error in zmq_socket: %s\n", zmq_strerror (errno));
		return -1;
	}

	rc = zmq_connect (s, connect_to);
	if (rc != 0) {
		printf ("error in zmq_connect: %s\n", zmq_strerror (errno));
		return -1;
	}

	rc = zmq_msg_init_size (&msg, message_size);
	if (rc != 0) {
		printf ("error in zmq_msg_init_size: %s\n", zmq_strerror (errno));
		return -1;
	}
	memset (zmq_msg_data (&msg), 0, message_size);

	watch = zmq_stopwatch_start ();
#endif

	for (int i = 0; i < thread_num; i++) {
		tid[i] = i;
		rc = pthread_create (&threads[i], NULL, worker, (void *)&tid[i]);
		if (rc != 0) {
			printf ("error in pthread_create: %s\n", zmq_strerror (rc));
			return -1;
		}
	}

	for (int i = 0; i < thread_num; i++) {
		rc = pthread_join (threads[i], NULL);
	}

	printf("done\n");

#if 0
	elapsed = zmq_stopwatch_stop (watch);
	if (elapsed == 0)
		elapsed = 1;

	throughput = 
		((double) message_count / (double) elapsed * 1000000);
	megabits = ((double) throughput * message_size * 8) / 1000000;

	printf ("message size: %d [B]\n", (int) message_size);
	printf ("message count: %d\n", (int) message_count);
	printf ("mean throughput: %d [msg/s]\n", (int) throughput);
	printf ("mean throughput: %.3f [Mb/s]\n", (double) megabits);
#endif
    return 0;
}
