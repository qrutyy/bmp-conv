// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "../../logger/log.h"
#include "../utils/threads-general.h"
#include "compute.h"

uint16_t st_next_x_block = 0;
uint16_t st_next_y_block = 0;
pthread_mutex_t st_xy_block_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *sthread_function(void *arg)
{
	struct thread_spec *th_spec = (struct thread_spec *)arg;
	int8_t result = 0;

	while (1) {
		switch ((enum compute_mode)th_spec->st_gen_info->args->compute_mode) {
		case BY_ROW:
			result = process_by_row(th_spec, &st_next_x_block, th_spec->st_gen_info->args->block_size, &st_xy_block_mutex);
			break;
		case BY_COLUMN:
			result = process_by_column(th_spec, &st_next_y_block, th_spec->st_gen_info->args->block_size, &st_xy_block_mutex);
			break;
		case BY_PIXEL:
			result = process_by_pixel(th_spec, &st_next_x_block, &st_next_y_block, &st_xy_block_mutex);
			break;
		case BY_GRID:
			result = process_by_grid(th_spec, &st_next_x_block, &st_next_y_block, th_spec->st_gen_info->args->block_size, &st_xy_block_mutex);
			break;
		default:
			log_error("Error: Invalid mode %d in thread function.\n", th_spec->st_gen_info->args->compute_mode);
			result = 1;
			break;
		}

		if (result != 0)
			goto exit;

		if (!th_spec || !th_spec->st_gen_info->args || !th_spec->st_gen_info->args->filter_type || !th_spec->st_gen_info->filters) {
			log_error("Error: Invalid state before filter_part_computation.\n");
			return NULL;
		}
		filter_part_computation(th_spec, th_spec->st_gen_info->args->filter_type, th_spec->st_gen_info->filters);
	}

exit:
	return NULL;
}

double execute_mt_computation(int threadnum, struct img_dim *dim, struct img_spec *img_spec, struct p_args *args, struct filter_mix *filters)
{
	pthread_t *th = NULL;
	double start_time = 0, end_time = 0;
	int8_t create_error = 0;
	size_t i = 0;
	struct thread_spec *th_spec[threadnum];

	th = malloc(threadnum * sizeof(pthread_t));
	if (!th) {
		goto mem_err;
	}

	// setup thread-locals details before thread creation
	for (i = 0; i < (size_t)threadnum; i++) {
		th_spec[i] = init_thread_spec(args, filters);
		if (!th_spec[i]) {
			create_error = -1;
			log_error("Memory allocation error for thread_spec %d\n", i);
			threadnum = i;
			break;
		}

		th_spec[i]->dim = dim;
		th_spec[i]->img = img_spec;
	}
	if (create_error < 0)
		goto mem_th_err;

	start_time = get_time_in_seconds();
	for (i = 0; i < (size_t)threadnum; i++) {
		if (pthread_create(&th[i], NULL, sthread_function, th_spec[i]) != 0) {
			log_error("Failed to create a thread");
			free(th_spec[i]->st_gen_info);
			free(th_spec[i]);
			create_error = 1;
			threadnum = i;
			break;
		}
	}

	for (i = 0; i < (size_t)threadnum; i++) {
		if (pthread_join(th[i], NULL)) {
			log_error("Failed to join a thread");
			break;
		}
		free(th_spec[i]->st_gen_info);
		free(th_spec[i]);
	}

	end_time = get_time_in_seconds();

	if (create_error) {
		free(th);
		return 0;
	}

	free(th);
	return end_time - start_time;

mem_err:
	log_error("Error: Memory allocation failed\n");
	return 0;

mem_th_err:
	for (i = 0; i < (size_t)threadnum; i++) {
		free(th_spec[i]);
	}
	return 0;
}
