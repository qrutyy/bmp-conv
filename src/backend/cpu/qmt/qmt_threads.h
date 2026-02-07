// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdint.h>

#define MAX_PATH_LEN 40

#include "utils/args-parse.h"
#include "utils/filters.h"
#include "qmt_queue.h"

// thread-work specific struct for better abstraction (#saynotoglobals)
struct threads_info {
	uint8_t used_threads;
	pthread_t *threads;
};

#ifndef PTHREAD_BARRIER_SERIAL_THREAD
#include "utils/pthread_barrier.h"
#endif

// Structure for incapsulating all the information needed while thread execution in queue-mode
struct qthreads_gen_info {
	struct threads_info *wot_info; // worker-thread count
	struct threads_info *ret_info; // reader-thread count
	struct threads_info *wrt_info; // writer-thread count
	struct p_args *pargs;
	struct filter_mix *filters;
	struct img_queue *output_q;
	struct img_queue *input_q;
#ifdef __APPLE__
	// macOS doesn't support pthread_barrier_t natively
	// You might need a custom implementation or use a different synchronization primitive
	// For now, let's assume a custom implementation or conditional compilation
	void *reader_barrier; 
#else
	pthread_barrier_t *reader_barrier; 
#endif
};

/**
 * Reads image file paths specified in arguments, loads the BMP images, and pushes them onto the input queue for worker threads.
 * Handles atomic incrementing of the global read file counter.
 * After processing all files, pushes termination signals onto the queue and waits on a barrier before sending signals.
 *
 * @param arg A pointer to a struct qthreads_gen_info containing shared information like program arguments, queues, and barriers.
 *
 * @return NULL after completion or in case of critical failure.
 */
void *reader_thread(void *arg);

/**
 * Main function for worker threads. Enters a loop to get tasks (images) from the input queue, allocate resources, process the image using filters and the specified compute mode, push the result to the output queue, log timing, and clean up resources for the completed task.
 * Exits the loop upon receiving a termination signal or encountering a queue error.
 *
 * @param arg A void pointer to a struct qthreads_gen_info containing shared information like program arguments, queues, and filter settings.
 *
 * @return NULL upon completion or exit signal.
 */
void *writer_thread(void *arg);

/**
 * Main function for writer threads. Enters a loop to get processed images (tasks) from the output queue, construct the output filename, write the image data to disk, log timing information, and free the image resources. Handles atomic updates to the global written file counter.
 * Exits when all expected files have been written or a queue error occurs.
 *
 * @param arg A void pointer to a struct qthreads_gen_info containing shared information like program arguments and the output queue.
 *
 * @return NULL upon completion or error.
 */
void *worker_thread(void *arg);
