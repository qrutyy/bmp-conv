// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../../libbmp/libbmp.h"
#include <pthread.h>
#include <stdint.h>

#define MAX_QUEUE_SIZE 2
#define MAX_QUEUE_MEMORY (50 * 1024 * 1024)
#define RAW_MEM_OVERHEAD (512 * 1024) // Assumed overhead for non-pixel data per image

struct queue_img_info {
	bmp_img *image;
	char *filename;
};

// queue_node struct
struct img_queue {
	struct queue_img_info *images[MAX_QUEUE_SIZE];
	uint8_t front, rear, size;
	pthread_mutex_t mutex;

	// for advanced balancing by mem_usage factor;
	pthread_cond_t cond_non_empty, cond_non_full;
	size_t current_mem_usage, max_mem_usage;
};

void queue_push(struct img_queue *q, bmp_img *img, char *filename);
void queue_init(struct img_queue *q, size_t max_mem);
void queue_destroy(struct img_queue *q);
bmp_img *queue_pop(struct img_queue *q, char **filename, uint8_t file_count, size_t *written_files);
