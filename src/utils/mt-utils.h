// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../../libbmp/libbmp.h"
#include "utils.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

enum compute_mode { BY_ROW, BY_COLUMN, BY_PIXEL, BY_GRID };

// i know that isn't necessary, just a way to make it cleaner
// (somewhere without 6 '->'), just an alias
struct img_dim {
	uint16_t height;
	uint16_t width;
};

struct img_spec {
	bmp_img *input_img;
	bmp_img *output_img;
};

// thread-specific parameters for computation
struct thread_spec {
	struct img_spec *img;
	struct img_dim *dim;
	uint16_t start_column;
	uint16_t start_row;
	uint16_t end_row;
	uint16_t end_column;
};

static size_t estimate_image_memory(const bmp_img *img);
uint8_t process_by_row(struct thread_spec *th_spec, uint16_t *next_x_block, uint16_t block_size, pthread_mutex_t *x_block_mutex);
uint8_t process_by_column(struct thread_spec *th_spec, uint16_t *next_y_block, uint16_t block_size, pthread_mutex_t *y_block_mutex);
uint8_t process_by_grid(struct thread_spec *th_spec, uint16_t *next_x_block, uint16_t *next_y_block, uint16_t block_size, pthread_mutex_t *xy_block_mutex);
uint8_t process_by_pixel(struct thread_spec *th_spec, uint16_t *next_x_block, uint16_t *next_y_block, pthread_mutex_t *xy_block_mutex);
void apply_filter(struct thread_spec *spec, struct filter cfilter);
void apply_median_filter(struct thread_spec *spec, uint16_t filter_size);
void filter_part_computation(struct thread_spec *spec, char* filter_type, struct filter_mix *filters);
struct img_dim *init_dimensions(uint16_t width, uint16_t height);
struct img_spec *init_img_spec(bmp_img *input, bmp_img *output);
void *thread_spec_init(void);
