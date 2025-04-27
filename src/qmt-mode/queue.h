// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../../libbmp/libbmp.h"
#include <pthread.h>
#include <stdint.h>

#define RAW_MEM_OVERHEAD (1) // Assumed overhead for non-pixel data per image

struct queue_img_info {
	bmp_img *image;
	char *filename;
};

// queue_node struct
struct img_queue {
	struct queue_img_info **images;
	uint32_t front, rear, size, capacity; // capacity - max el count
	pthread_mutex_t mutex;

	// for advanced balancing by mem_usage factor;
	pthread_cond_t cond_non_empty, cond_non_full;
	size_t current_mem_usage, max_mem_usage; // in mb
};

/**
 * Initializes a thread-safe image queue structure.
 * Sets initial queue state (size, front, rear), memory usage limits,
 * and initializes the mutex and condition variables for synchronization.
 *
 * @param q A pointer to the img_queue structure to be initialized.
 * @param max_mem The maximum total estimated memory (in bytes) the queue should hold across all images. If 0, a default maximum is used.
 */
int queue_init(struct img_queue *q, uint32_t capacity, size_t max_mem);

/**
 * Pushes an image and its associated filename onto the thread-safe queue.
 * Blocks if the queue is full (either by item count or estimated memory usage)
 * until space becomes available. Estimates image memory usage before adding.
 * Handles memory allocation for queue metadata and signals waiting consumers.
 *
 * @param q - A pointer to the img_queue structure.
 * @param img - A pointer to the bmp_img structure to be added. Ownership is transferred.
 * @param filename - A string containing the filename associated with the image. Ownership is transferred (the pointer itself, not usually a copy). Must not be NULL (function returns early if it is).
 * @param mode - A pointer to mode string.
 */
void queue_push(struct img_queue *q, bmp_img *img, char *filename, const char *mode);

/**
 * Destroys the initialised primitives for queue-func (mutex and cond_vars).
 *
 * @param q A pointer to the img_queue structure to be initialized.
 */
void queue_destroy(struct img_queue *q);

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
bmp_img *queue_pop(struct img_queue *q, char **filename, uint8_t file_count, size_t *written_files, const char *mode);
