// SPDX-License-Identifier: GPL-3.0-or-later

#include "queue.h"
#include "threads.h"
#include "exec.h"
#include "../utils/args-parse.h"
#include "../utils/utils.h"
#include <pthread.h>
#include <string.h>
#include <stdatomic.h>
#include <errno.h>

/** Notes about balanced distributed work:
  * 1. Its kinda already balanced when the size of the queue is limited.
  * 2. We can calculate the ammount of used per thread memory/pixels and limit by it (so it would be fairly balanced)...
  * 3. Memory usage per thread isn't a good metric in terms of metrics that directly depend on execution time. However, at first i won't depend on threadnum, block_size and other metrics that affect the execution time.
*/


// due to queue_pop sleep mechanism - we should use global atomic for writers and writers, to prevent the deadlock-safety

pthread_barrier_t reader_barrier;

// yeah, its not that precise. baytik tuda syuda...

int allocate_qthread_resources(struct qthreads_gen_info *qt_info, struct p_args *args_ptr, struct img_queue *input_queue, struct img_queue *output_queue)
{
	size_t q_mem_limit = 0;

	qt_info->ret_info = malloc(sizeof(struct threads_info));
	qt_info->wrt_info = malloc(sizeof(struct threads_info));
	qt_info->wot_info = malloc(sizeof(struct threads_info));

	qt_info->wot_info->threads = malloc(args_ptr->wot_count * sizeof(pthread_t));
	if (!qt_info->wot_info->threads && args_ptr->wot_count > 0) {
		goto mem_err;
	}

	qt_info->ret_info->threads = malloc(args_ptr->ret_count * sizeof(pthread_t));
	if (!qt_info->ret_info->threads && args_ptr->ret_count > 0) {
		free(qt_info->ret_info->threads);
		qt_info->ret_info->threads = NULL;
		goto mem_err;
	}

	qt_info->wrt_info->threads = malloc(args_ptr->wrt_count * sizeof(pthread_t));
	if (!qt_info->wrt_info->threads && args_ptr->wrt_count > 0) {
		free(qt_info->ret_info->threads);
		qt_info->ret_info->threads = NULL;
		free(qt_info->wot_info->threads);
		qt_info->wot_info->threads = NULL;
		goto mem_err;
	}

	q_mem_limit = args_ptr->queue_memory_limit > 0 ? args_ptr->queue_memory_limit : MAX_QUEUE_MEMORY;

	queue_init(input_queue, q_mem_limit);
	queue_init(output_queue, q_mem_limit);

	qt_info->pargs = args_ptr;
	qt_info->input_q = input_queue;
	qt_info->output_q = output_queue;

	return 0;

mem_err:
	fprintf(stderr, "Error: memory allocation failed at allocate_qthread_resources\n");
	return -1;
}

// yes, too much args, but at what cost ;)
void create_qthreads(struct qthreads_gen_info *qt_info, struct p_args *args_ptr)
{
	size_t i;
	printf("Creating %hhu readers, %hhu workers, %hhu writers\n", args_ptr->ret_count, args_ptr->wot_count, args_ptr->wrt_count);

	pthread_barrier_init(&reader_barrier, NULL, qt_info->pargs->ret_count);

	for (i = 0; i < args_ptr->ret_count; i++) {
		if (pthread_create(&qt_info->ret_info->threads[i], NULL, reader_thread, qt_info)) {
			perror("Failed to create a reader thread");
			break;
		}
		qt_info->ret_info->used_threads++;
	}

	for (i = 0; i < args_ptr->wot_count; i++) {
		if (pthread_create(&qt_info->wot_info->threads[i], NULL, worker_thread, qt_info)) {
			perror("Failed to create a worker thread");
			break;
		}
		qt_info->wot_info->used_threads++;
	}

	for (i = 0; i < args_ptr->wrt_count; i++) {
		if (pthread_create(&qt_info->wrt_info->threads[i], NULL, writer_thread, qt_info)) {
			perror("Failed to create a writer thread");
			break;
		}
		qt_info->wrt_info->used_threads++;
	}
}

void join_qthreads(struct qthreads_gen_info *qt_info)
{
	size_t i;
	for (i = 0; i < qt_info->ret_info->used_threads; i++) {
		if (pthread_join(qt_info->ret_info->threads[i], NULL)) {
			fprintf(stderr, "Failed to join a reader thread");
		}
	}

	for (i = 0; i < qt_info->wot_info->used_threads; i++) {
		if (pthread_join(qt_info->wot_info->threads[i], NULL)) {
			fprintf(stderr, "Failed to join a worker thread");
		}
	}

	for (i = 0; i < qt_info->wrt_info->used_threads; i++) {
		if (pthread_join(qt_info->wrt_info->threads[i], NULL)) {
			fprintf(stderr, "Failed to join a writer thread");
		}
	}
	pthread_barrier_destroy(&reader_barrier);
}

void free_qthread_resources(struct qthreads_gen_info *qt_info)
{
	free(qt_info->wot_info->threads);
	free(qt_info->ret_info->threads);
	free(qt_info->wrt_info->threads);
	free(qt_info->wot_info);
	free(qt_info->ret_info);
	free(qt_info->wrt_info);
	free(qt_info);
}

void qt_write_logs(double result_time, enum LOG_TAG tag)
{
	FILE *file = NULL;

	file = fopen(QT_LOG_FILE_PATH, "a");
	const char *log_tag_str = log_tag_to_str(tag);

	if (file) {
		fprintf(file, "%s %.6f\n", log_tag_str, result_time);
		fclose(file);
	} else {
		fputs("Error: could not open timing results file for appending.\n", stderr);
	}

	return;
}
