// SPDX-License-Identifier: GPL-3.0-or-later

#include <pthread.h>
#include <stdint.h>

#pragma once 

#define MAX_PATH_LEN 40

// thread-work specific struct for better abstraction (#saynotoglobals)
struct threads_info {
	uint8_t used_threads;
	pthread_t *threads;
};

// Structure for incapsulating all the information needed while thread execution in queue-mode
struct qthreads_gen_info {
	struct threads_info *wot_info; // worker-thread count
	struct threads_info *ret_info; // reader-thread count
	struct threads_info *wrt_info; // writer-thread count
	struct p_args *pargs;
	struct filter_mix *filters;
	struct img_queue *output_q;
	struct img_queue *input_q;
	pthread_barrier_t *reader_barrier; // kinda not general (doesn't suit worker and writer threads, but is included to avoid cyclic deps)
};

void *reader_thread(void *arg);
void *writer_thread(void *arg);
void *worker_thread(void *arg);

