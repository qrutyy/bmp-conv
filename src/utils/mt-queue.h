// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../libbmp/libbmp.h"
#include <pthread.h>
#include <stdint.h>

#define MAX_QUEUE_SIZE 10
#define MAX_PATH_LEN 40
#define MAX_QUEUE_MEMORY (50 * 1024 * 1024)

struct queue_img_info {
	bmp_img *image;
	char *filename;
};

struct img_queue {
	struct queue_img_info *images[MAX_QUEUE_SIZE];
	uint8_t front, rear, size;
	pthread_mutex_t mutex;

	// for advanced balancing by mem_usage factor;
	pthread_cond_t cond_non_empty, cond_non_full;
	size_t current_mem_usage, max_mem_usage;
};

// thread-work specific struct for better abstraction (#saynotoglobals)
struct threads_info {
	uint8_t used_threads;
	pthread_t *threads;
};

// all the information needed while thread execution in queue-mode
struct qthreads_info {
	struct threads_info *wot_info;
	struct threads_info *ret_info;
	struct threads_info *wrt_info;
	struct p_args *pargs;
	struct img_queue *output_q;
	struct img_queue *input_q;
	struct filter_mix *filters;
};

void queue_init(struct img_queue *q, size_t max_mem);

void *reader_thread(void *arg);
void *worker_thread(void *arg);
void *writer_thread(void *arg);

int allocate_qthread_resources(struct qthreads_info *qt_info, struct p_args *args_ptr, struct img_queue *input_queue, struct img_queue *output_queue);
void join_qthreads(struct qthreads_info *qt_info);
void create_qthreads(struct qthreads_info *qt_info, struct p_args *args_ptr);
void free_qthread_resources(struct qthreads_info *qt_info);
