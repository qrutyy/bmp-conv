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
 * @return The estimated memory usage in MB.
 */
static size_t estimate_image_memory(const bmp_img *img)
{
	size_t bytes_per_pixel, pixel_data_size_bytes, total_bytes;
	size_t megabytes;

	bytes_per_pixel = img->img_header.biBitCount / 8;
	if (bytes_per_pixel == 0)
		bytes_per_pixel = 3;

	pixel_data_size_bytes = (size_t)img->img_header.biWidth * img->img_header.biHeight * bytes_per_pixel;

	total_bytes = pixel_data_size_bytes + sizeof(bmp_img);
	if (img->img_pixels != NULL) {
		total_bytes += (size_t)img->img_header.biHeight * sizeof(bmp_pixel *) + RAW_MEM_OVERHEAD;
	}

	megabytes = (total_bytes + (1024 * 1024 - 1)) / (1024 * 1024);
	megabytes += RAW_MEM_OVERHEAD;

	if (megabytes == 0) {
		megabytes = 1;
	}

	return megabytes;
}

int queue_init(struct img_queue *q, uint32_t capacity, size_t max_mem)
{
	q->front = q->rear = q->size = 0;
	q->current_mem_usage = 0;
	q->capacity = capacity;
	q->max_mem_usage = max_mem;

	q->images = malloc(capacity * sizeof(struct queue_img_info *));
	if (!q->images) {
		log_error("Failed to allocate queue image array (capacity: %u)", capacity);
		return -1;
	}

	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->cond_non_empty, NULL);
	pthread_cond_init(&q->cond_non_full, NULL);
	log_info("Queue initialized with max memory: %zu MB", q->max_mem_usage);
	return 0;
}

void queue_destroy(struct img_queue *q)
{
	if (!q)
		return;

	pthread_mutex_lock(&q->mutex);

	log_debug("Destroying queue: Capacity=%u, Size=%u, MemUsage=%zu MiB", q->capacity, q->size, q->current_mem_usage);

	while (q->size > 0) {
		uint32_t current_front = q->front;
		struct queue_img_info *iqi = q->images[current_front];

		q->front = (q->front + 1) % q->capacity;
		q->size--;

		if (iqi) {
			log_trace("Destroying remaining queue element: filename='%s'", iqi->filename ? iqi->filename : "NULL");
			if (iqi->image) {
				if (iqi->image->img_header.biWidth > 0 || iqi->image->img_header.biHeight > 0) {
					bmp_img_free(iqi->image);
				}
				free(iqi->image);
				iqi->image = NULL;
			}
			if (iqi->filename) {
				free(iqi->filename);
				iqi->filename = NULL;
			}
			free(iqi);
			q->images[current_front] = NULL;
		} else {
			log_warn("Found NULL element pointer in queue during destroy at index %u", current_front);
		}
	}

	if (q->size != 0) {
		log_error("Queue size is non-zero (%u) after cleanup in destroy!", q->size);
	}
	q->current_mem_usage = 0;

	free(q->images);
	q->images = NULL;

	pthread_mutex_unlock(&q->mutex);

	pthread_mutex_destroy(&q->mutex);
	pthread_cond_destroy(&q->cond_non_full);
	pthread_cond_destroy(&q->cond_non_empty);
	log_info("Queue destroyed successfully.");
}

void queue_push(struct img_queue *q, bmp_img *img, char *filename, const char *mode)
{
	struct queue_img_info *iq_info = NULL;
	size_t image_memory = 0;
	double start_block_time = 0;
	double result_time = 0;
	int8_t is_arr_limit = 0, is_mem_limit = 0;

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
	log_trace("Pushing '%s', estimated memory: %zu MB. Current usage: %zu/%zu, size: %u/%u", filename, image_memory, q->current_mem_usage, q->max_mem_usage, q->size,
		  q->capacity);

	is_mem_limit = (q->current_mem_usage + image_memory > q->max_mem_usage && q->size > 0);
	is_arr_limit = (q->size >= q->capacity); // check limit

	while (is_mem_limit || is_arr_limit) {
		if (is_arr_limit) {
			log_debug("Queue array full (size %u >= MAX_QUEUE_SIZE %d). Waiting...", q->size, q->capacity);
		} else { // is_mem_limit must be true
			log_debug("Queue memory limit would be exceeded (current: %zu + new: %zu > max: %zu). Waiting...", q->current_mem_usage, image_memory, q->max_mem_usage);
		}
		start_block_time = get_time_in_seconds();
		pthread_cond_wait(&q->cond_non_full, &q->mutex);
		log_trace("Woke up from cond_non_full wait.");

		// re-check
		is_mem_limit = (q->current_mem_usage + image_memory > q->max_mem_usage && q->size > 0);
		is_arr_limit = (q->size >= q->capacity);
	}

	result_time = (start_block_time != 0) ? get_time_in_seconds() - start_block_time : 0;
	if (result_time > 0) {
		log_trace("Blocked on push for %.4f seconds.", result_time);
		qt_write_logs(result_time, QPUSH, mode);
	}

	q->images[q->rear] = iq_info;
	q->rear = (q->rear + 1) % q->capacity;
	q->size++;
	q->current_mem_usage += image_memory;

	log_trace("Pushed '%s'. New usage: %zu MB, size: %zu", filename, q->current_mem_usage, q->size);

	pthread_cond_signal(&q->cond_non_empty);
	pthread_mutex_unlock(&q->mutex);
}

bmp_img *queue_pop(struct img_queue *q, char **filename, uint8_t file_count, size_t *written_files, const char *mode)
{
	struct queue_img_info *iqi = NULL;
	bmp_img *img_src = NULL;
	size_t image_memory = 0;
	double start_block_time = 0;
	double result_time = 0;
	struct timespec wait_time;
	uint8_t curr_written_files = 0;
	int wait_result = 0;
	*filename = NULL;

	pthread_mutex_lock(&q->mutex);

restart_wait_loop:
	while (q->size == 0) {
		start_block_time = get_time_in_seconds();
		
		if ((curr_written_files = __atomic_load_n(written_files, __ATOMIC_ACQUIRE)) >= file_count) {
			log_debug("Pop termination check: written_files (%zu) >= file_count (%u). Returning NULL.", curr_written_files, file_count);
			pthread_mutex_unlock(&q->mutex);
			return NULL;
		}

		log_trace("Queue empty, waiting on cond_non_empty...");
		set_wait_time(&wait_time);
		wait_result = pthread_cond_timedwait(&q->cond_non_empty, &q->mutex, &wait_time);

		if (wait_result == ETIMEDOUT) {
			log_trace("Consumer timed out waiting for item.");
			if ((curr_written_files = __atomic_load_n(written_files, __ATOMIC_ACQUIRE)) >= file_count) {
				log_debug("Pop termination check after timeout: written_files (%zu) >= file_count (%u). Returning NULL.",
					  curr_written_files, file_count);
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
		qt_write_logs(result_time, QPOP, mode);
	}

	iqi = q->images[q->front];
	image_memory = estimate_image_memory(iqi->image);

	q->front = (q->front + 1) % q->capacity;
	q->size--;
	if (q->current_mem_usage >= image_memory) {
		q->current_mem_usage -= image_memory;
	} else {
		q->current_mem_usage = 0;
	}

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
