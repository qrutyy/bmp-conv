// SPDX-License-Identifier: GPL-3.0-or-later

#include "../utils/args-parse.h"
#include "../../logger/log.h"
#include "queue.h"
#include "threads.h"
#include "exec.h"
#include <pthread.h>
#include <string.h>
#include <stdatomic.h>
#include <errno.h>

/** Notes about balanced distributed work:
 * 1. Its kinda already balanced when the size of the queue is limited. Btw we can't guess the optimal limitation.
 * It should be >= CPU core number, for better core utilisation.
 * 2. We can calculate the ammount of used per thread memory/pixels and limit by it (so it would be fairly balanced)...
 * 3. Memory usage per thread isn't a good metric in terms of metrics that directly depend on execution time. However, at first i won't depend on threadnum, block_size and other metrics that affect the execution time.
 */

int allocate_qthread_resources(struct qthreads_gen_info *qt_info, struct p_args *args_ptr, struct img_queue *input_queue, struct img_queue *output_queue)
{
	size_t q_mem_limit = 0;

	qt_info->ret_info = malloc(sizeof(struct threads_info));
	qt_info->wrt_info = malloc(sizeof(struct threads_info));
	qt_info->wot_info = malloc(sizeof(struct threads_info));

	if (!qt_info->ret_info || !qt_info->wrt_info || !qt_info->wot_info) {
		free(qt_info->ret_info);
		free(qt_info->wrt_info);
		free(qt_info->wot_info);
		goto mem_err;
	}

	qt_info->ret_info->used_threads = 0;
	qt_info->wrt_info->used_threads = 0;
	qt_info->wot_info->used_threads = 0;
	qt_info->ret_info->threads = NULL;
	qt_info->wrt_info->threads = NULL;
	qt_info->wot_info->threads = NULL;

	if (args_ptr->wot_count > 0) {
		qt_info->wot_info->threads = malloc(args_ptr->wot_count * sizeof(pthread_t));
		if (!qt_info->wot_info->threads)
			goto mem_err_cleanup;
	}
	if (args_ptr->ret_count > 0) {
		qt_info->ret_info->threads = malloc(args_ptr->ret_count * sizeof(pthread_t));
		if (!qt_info->ret_info->threads)
			goto mem_err_cleanup;
	}
	if (args_ptr->wrt_count > 0) {
		qt_info->wrt_info->threads = malloc(args_ptr->wrt_count * sizeof(pthread_t));
		if (!qt_info->wrt_info->threads)
			goto mem_err_cleanup;
	}

	q_mem_limit = args_ptr->queue_memory_limit_mb > 0 ? args_ptr->queue_memory_limit_mb : DEFAULT_QUEUE_MEM_LIMIT;
	queue_init(input_queue, q_mem_limit, args_ptr->queue_memory_limit_mb);
	queue_init(output_queue, q_mem_limit, args_ptr->queue_memory_limit_mb);

	qt_info->pargs = args_ptr;
	qt_info->input_q = input_queue;
	qt_info->output_q = output_queue;

	qt_info->reader_barrier = malloc(sizeof(pthread_barrier_t));

	return 0;

mem_err_cleanup: // Cleanup if thread arrays failed after info structs succeeded
	free(qt_info->ret_info->threads);
	free(qt_info->wot_info->threads);
	free(qt_info->wrt_info->threads);
	free(qt_info->ret_info);
	free(qt_info->wrt_info);
	free(qt_info->wot_info);

mem_err: // General memory error entry point
	log_error("Memory allocation failed in allocate_qthread_resources");
	qt_info->ret_info = NULL;
	qt_info->wrt_info = NULL;
	qt_info->wot_info = NULL;
	return -1;
}

void create_qthreads(struct qthreads_gen_info *qt_info)
{
	size_t i = 0;
	int ret = 0;

	log_info("Creating %hhu readers, %hhu workers, %hhu writers", qt_info->pargs->ret_count, qt_info->pargs->wot_count, qt_info->pargs->wrt_count);

	if (qt_info->pargs->ret_count > 0) {
		ret = pthread_barrier_init(qt_info->reader_barrier, NULL, qt_info->pargs->ret_count);
		if (ret != 0) {
			log_error("Failed to initialize reader barrier: %s", strerror(ret));
			return; // Cannot proceed without barrier if readers exist
		}
	}

	for (i = 0; i < qt_info->pargs->ret_count; i++) {
		ret = pthread_create(&qt_info->ret_info->threads[i], NULL, reader_thread, qt_info);
		if (ret != 0) {
			log_error("Failed to create reader thread %zu: %s", i, strerror(ret));
			break; // Stop creating more threads on failure
		}
		qt_info->ret_info->used_threads++;
	}

	for (i = 0; i < qt_info->pargs->wot_count; i++) {
		ret = pthread_create(&qt_info->wot_info->threads[i], NULL, worker_thread, qt_info);
		if (ret != 0) {
			log_error("Failed to create worker thread %zu: %s", i, strerror(ret));
			break;
		}
		qt_info->wot_info->used_threads++;
	}

	for (i = 0; i < qt_info->pargs->wrt_count; i++) {
		ret = pthread_create(&qt_info->wrt_info->threads[i], NULL, writer_thread, qt_info);
		if (ret != 0) {
			log_error("Failed to create writer thread %zu: %s", i, strerror(ret));
			break;
		}
		qt_info->wrt_info->used_threads++;
	}
	log_info("Launched %zu readers, %zu workers, %zu writers", qt_info->ret_info->used_threads, qt_info->wot_info->used_threads, qt_info->wrt_info->used_threads);
}

void join_qthreads(struct qthreads_gen_info *qt_info)
{
	size_t i = 0;
	int ret = 0;

	log_debug("Joining %zu reader threads...", qt_info->ret_info->used_threads);
	for (i = 0; i < qt_info->ret_info->used_threads; i++) {
		ret = pthread_join(qt_info->ret_info->threads[i], NULL);
		if (ret != 0) {
			log_error("Failed to join reader thread %zu: %s", i, strerror(ret));
		}
	}

	if (qt_info->pargs->ret_count > 0) {
		ret = pthread_barrier_destroy(qt_info->reader_barrier);
		if (ret != 0) {
			log_error("Failed to destroy reader barrier: %s", strerror(ret));
		}
	}

	log_debug("Joining %zu worker threads...", qt_info->wot_info->used_threads);
	for (i = 0; i < qt_info->wot_info->used_threads; i++) {
		ret = pthread_join(qt_info->wot_info->threads[i], NULL);
		if (ret != 0) {
			log_error("Failed to join worker thread %zu: %s", i, strerror(ret));
		}
	}

	log_debug("Joining %zu writer threads...", qt_info->wrt_info->used_threads);
	for (i = 0; i < qt_info->wrt_info->used_threads; i++) {
		ret = pthread_join(qt_info->wrt_info->threads[i], NULL);
		if (ret != 0) {
			log_error("Failed to join writer thread %zu: %s", i, strerror(ret));
		}
	}
	log_info("All threads joined.");
}

void free_qthread_resources(struct qthreads_gen_info *qt_info)
{
	if (!qt_info)
		return;

	log_debug("Freeing qthread resources.");
	if (qt_info->wot_info)
		free(qt_info->wot_info->threads);
	if (qt_info->ret_info)
		free(qt_info->ret_info->threads);
	if (qt_info->wrt_info)
		free(qt_info->wrt_info->threads);

	free(qt_info->wot_info);
	free(qt_info->ret_info);
	free(qt_info->wrt_info);

	queue_destroy(qt_info->input_q);
	queue_destroy(qt_info->output_q);

	free(qt_info);
}
