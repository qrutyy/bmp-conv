// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../../libbmp/libbmp.h"
#include "../utils/utils.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

enum compute_mode { BY_ROW, BY_COLUMN, BY_PIXEL, BY_GRID };

 // simple threads general info
struct sthreads_gen_info {	
	struct p_args *args;
	struct filter_mix *filters;
};

uint8_t process_by_row(struct thread_spec *th_spec, uint16_t *next_x_block, uint16_t block_size, pthread_mutex_t *x_block_mutex);
uint8_t process_by_column(struct thread_spec *th_spec, uint16_t *next_y_block, uint16_t block_size, pthread_mutex_t *y_block_mutex);
uint8_t process_by_grid(struct thread_spec *th_spec, uint16_t *next_x_block, uint16_t *next_y_block, uint16_t block_size, pthread_mutex_t *xy_block_mutex);
uint8_t process_by_pixel(struct thread_spec *th_spec, uint16_t *next_x_block, uint16_t *next_y_block, pthread_mutex_t *xy_block_mutex);

void *thread_spec_init(struct p_args *args, struct filter_mix *filters);
