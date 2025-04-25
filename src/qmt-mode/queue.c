// SPDX-License-Identifier: GPL-3.0-or-later

#include "queue.h"
#include "../utils/utils.h" // For get_time_in_seconds, qt_write_logs, set_wait_time
#include "../../logger/log.h"
#include <pthread.h>
#include <string.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

/**
 * Estimates the memory usage of a single BMP image structure and its pixel data.
 * Includes the size of the main struct, pixel data array, row pointers (if applicable),
 * and a fixed overhead assumption.
 *
 * @param img A pointer to the bmp_img structure whose memory usage is to be estimated.
 * 
 * @return The estimated memory usage in bytes.
 */
static size_t estimate_image_memory(const bmp_img *img)
{
	size_t bytes_per_pixel, pixel_data_size, bmp_struct_size, row_pointers_size = 0;

	bytes_per_pixel = img->img_header.biBitCount / 8;
	if (bytes_per_pixel == 0)
		bytes_per_pixel = 1;

	pixel_data_size = (size_t)img->img_header.biWidth * img->img_header.biHeight * bytes_per_pixel;
	bmp_struct_size = sizeof(bmp_img);

	if (img->img_pixels) {
		// Assuming img_pixels is an array of pointers to rows
		row_pointers_size = (size_t)img->img_header.biHeight * sizeof(bmp_pixel *);
	}

	return pixel_data_size + row_pointers_size + bmp_struct_size + RAW_MEM_OVERHEAD;
}

/**
 * Initializes a thread-safe image queue structure.
 * Sets initial queue state (size, front, rear), memory usage limits,
 * and initializes the mutex and condition variables for synchronization.
 *
 * @param q A pointer to the img_queue structure to be initialized.
 * @param max_mem The maximum total estimated memory (in bytes) the queue should hold across all images. If 0, a default maximum is used.
 */
void queue_init(struct img_queue *q, size_t max_mem)
{
	q->front = q->rear = q->size = 0;
	q->current_mem_usage = 0;
	q->max_mem_usage = max_mem;
	if (q->max_mem_usage == 0) {
		q->max_mem_usage = MAX_QUEUE_MEMORY;
	}
	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->cond_non_empty, NULL);
	pthread_cond_init(&q->cond_non_full, NULL);
	log_info("Queue initialized with max memory: %zu bytes", q->max_mem_usage);
}

/**
 * Destroys the initialised primitives for queue-func (mutex and cond_vars).
 *
 * @param q A pointer to the img_queue structure to be initialized.
 */
void queue_destroy(struct img_queue *q) {
	pthread_mutex_destroy(&q->mutex);
	pthread_cond_destroy(&q->cond_non_full);
	pthread_cond_destroy(&q->cond_non_empty);
}

/**
 * Pushes an image and its associated filename onto the thread-safe queue.
 * Blocks if the queue is full (either by item count or estimated memory usage)
 * until space becomes available. Estimates image memory usage before adding.
 * Handles memory allocation for queue metadata and signals waiting consumers.
 *
 * @param q A pointer to the img_queue structure.
 * @param img A pointer to the bmp_img structure to be added. Ownership is transferred.
 * @param filename A string containing the filename associated with the image. Ownership is transferred (the pointer itself, not usually a copy). Must not be NULL (function returns early if it is).
 */
void queue_push(struct img_queue *q, bmp_img *img, char *filename)
{
	struct queue_img_info *iq_info = NULL;
	size_t image_memory = 0;
	double start_block_time = 0;
	double result_time = 0;

	if (!filename) {
		log_warn("queue_push attempted with NULL filename. Skipping.");
		return;
	}
	if (!img) {
		log_warn("queue_push attempted with NULL image for filename '%s'. Skipping.", filename);
		return;
	}

	iq_info = malloc(sizeof(struct queue_img_info));
	if (!iq_info) {
		log_error("Memory allocation failed for queue_img_info in 'queue_push'");
		exit(EXIT_FAILURE);
	}
	iq_info->filename = filename;
	iq_info->image = img;

	pthread_mutex_lock(&q->mutex);

	image_memory = estimate_image_memory(img);
	log_trace("Pushing '%s', estimated memory: %zu bytes. Current usage: %zu/%zu, size: %zu/%d", filename, image_memory, q->current_mem_usage, q->max_mem_usage, q->size,
		  MAX_QUEUE_SIZE);

	// Wait while queue is full (by count or memory limit).
	// The memory condition allows at least one item even if it exceeds the limit initially.
	while (q->size >= MAX_QUEUE_SIZE || (q->current_mem_usage + image_memory > q->max_mem_usage && q->size > 0)) {
		log_debug("Queue full (size: %zu, mem: %zu + %zu > %zu). Waiting...", q->size, q->current_mem_usage, image_memory, q->max_mem_usage);
		start_block_time = get_time_in_seconds();
		pthread_cond_wait(&q->cond_non_full, &q->mutex);
		log_trace("Woke up from cond_non_full wait.");
	}

	result_time = (start_block_time != 0) ? get_time_in_seconds() - start_block_time : 0;
	if (result_time > 0) {
		log_trace("Blocked on push for %.4f seconds.", result_time);
		qt_write_logs(result_time, QPUSH);
	}

	q->images[q->rear] = iq_info;
	q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
	q->size++;
	q->current_mem_usage += image_memory;

	log_trace("Pushed '%s'. New usage: %zu bytes, size: %zu", filename, q->current_mem_usage, q->size);

	pthread_cond_signal(&q->cond_non_empty);
	pthread_mutex_unlock(&q->mutex);
}

/**
 * Pops an image and its filename from the thread-safe queue.
 * Blocks with a timeout if the queue is empty, checking periodically if all
 * expected files have been processed (based on written_files counter).
 * Returns NULL if the queue remains empty after timeout and all files are done,
 * or if a signal indicates completion. Allocates memory for the returned filename.
 *
 * !NOTE: in CI (Helgrind analysis) you may have noticed an error: 
 * "Thread #?: pthread_cond{signal,broadcast}: dubious: associated lock is not held by any thread"
 * Error indicates, that mutex inside queue_pop wasn't held before pthread_cond_timedwait was called. Thats obviously false 
 *
 * @param q A pointer to the img_queue structure.
 * @param filename A pointer to a char pointer (`char **`). On success, this will be updated to point to a newly allocated string containing the filename. The caller is responsible for freeing this memory.
 * @param file_count The total number of files expected to be processed by the system.
 * @param written_files A pointer to a global atomic counter tracking the number of files successfully processed (written). Used for termination check.
 * 
 * @return A pointer to the popped bmp_img structure, or NULL if the queue is empty and processing should terminate, or on error during timed wait.
 */
bmp_img *queue_pop(struct img_queue *q, char **filename, uint8_t file_count, size_t *written_files)
{
	struct queue_img_info *iqi = NULL;
	bmp_img *img_src = NULL;
	size_t image_memory = 0;
	double start_block_time = 0;
	double result_time = 0;
	struct timespec wait_time;
	int wait_result = 0;
	*filename = NULL;

	pthread_mutex_lock(&q->mutex);

restart_wait_loop:
	while (q->size == 0) {
		start_block_time = get_time_in_seconds();
		if (__atomic_load_n(written_files, __ATOMIC_ACQUIRE) >= file_count) {
			log_debug("Pop termination check: written_files (%zu) >= file_count (%u). Returning NULL.", __atomic_load_n(written_files, __ATOMIC_ACQUIRE), file_count);
			pthread_mutex_unlock(&q->mutex);
			return NULL;
		}

		log_trace("Queue empty, waiting on cond_non_empty...");
		set_wait_time(&wait_time);
		wait_result = pthread_cond_timedwait(&q->cond_non_empty, &q->mutex, &wait_time);

		if (wait_result == ETIMEDOUT) {
			log_trace("Consumer timed out waiting for item.");
			if (__atomic_load_n(written_files, __ATOMIC_ACQUIRE) >= file_count) {
				log_debug("Pop termination check after timeout: written_files (%zu) >= file_count (%u). Returning NULL.",
					  __atomic_load_n(written_files, __ATOMIC_ACQUIRE), file_count);
				pthread_mutex_unlock(&q->mutex);
				return NULL;
			}
			goto restart_wait_loop; // Try waiting again if not finished
		} else if (wait_result != 0) {
			log_error("pthread_cond_timedwait error: %s", strerror(wait_result));
			pthread_mutex_unlock(&q->mutex);
			return NULL;
		}
		log_trace("Consumer woken up by signal."); // Normal wakeup, re-check q->size
	}

	result_time = (start_block_time != 0) ? get_time_in_seconds() - start_block_time : 0;
	if (result_time > 0) {
		log_trace("Blocked on pop for %.4f seconds.", result_time);
		qt_write_logs(result_time, QPOP);
	}

	iqi = q->images[q->front];
	image_memory = estimate_image_memory(iqi->image);

	q->front = (q->front + 1) % MAX_QUEUE_SIZE;
	q->size--;
	q->current_mem_usage -= image_memory;

	log_trace("Popped '%s'. New usage: %zu bytes, size: %zu", (iqi->filename ? iqi->filename : "NULL"), q->current_mem_usage, q->size);

	if (iqi->filename) {
		*filename = strdup(iqi->filename);
		if (!*filename) {
			log_error("strdup failed for filename in queue_pop");
			free(iqi);
			pthread_cond_signal(&q->cond_non_full);
			pthread_mutex_unlock(&q->mutex);
			return NULL;
		}
	} else {
		*filename = NULL;
	}

	img_src = iqi->image;
	free(iqi);

	pthread_cond_signal(&q->cond_non_full);
	pthread_mutex_unlock(&q->mutex);

	return img_src;
}
