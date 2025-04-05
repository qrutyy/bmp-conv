// SPDX-License-Identifier: GPL-3.0-or-later

#include "../libbmp/libbmp.h"
#include "utils/mt-utils.h"
#include "utils/utils.h"
#include "utils/mt-queue.h"
#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <sys/time.h>

#define LOG_FILE_PATH "tests/timing-results.dat"

struct filter_mix *filters = NULL;
struct p_args *args = NULL;

uint16_t next_x_block = 0;
uint16_t next_y_block = 0;
pthread_mutex_t xy_block_mutex = PTHREAD_MUTEX_INITIALIZER;

//shorten this up
static int parse_args(int argc, char *argv[])
{
	uint8_t rww_found = 0;
	uint8_t parsed_count = 0;
	char *rww_values = NULL;
	int wrt_temp, ret_temp, wot_temp = 0;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <input_file.bmp> --mode=<compute_mode> --filter=<type>  --threadnum=<N> --block=<size> [--output=<file>]\n", argv[0]);
		return -1;
	}

	initialize_args(args);

	if (strncmp(argv[1], "-queue-mode", 6) == 0)
		args->queue_mode = 1;

	// parsing mandatory args
	for (int i = 2; i < argc; i++) {
		if (strncmp(argv[i], "--filter=", 9) == 0) {
			args->filter_type = check_filter_arg(argv[i] + 9);
			if (!args->filter_type)
				return -1;
			argv[i] = "_";
		} else if (strncmp(argv[i], "--mode=", 7) == 0) {
			args->compute_mode = check_mode_arg(argv[i] + 7);
			if (args->compute_mode < 0)
				return -1;
			argv[i] = "_";
		} else if (strncmp(argv[i], "--block=", 8) == 0) {
			args->block_size = atoi(argv[i] + 8);
			if (args->block_size <= 0) {
				fprintf(stderr, "Error: Block size must be >= 1.\n");
				return -1;
			}
			argv[i] = "_";
		}
		// special character, that signifies for already parsed mandatory arg
	}

	if (args->queue_mode) {
		for (int i = 2; i < argc; i++) {
			if (strncmp(argv[i], "--log=", 6) == 0) {
				args->log_enabled = atoi(argv[i] + 6);
			} else if (strncmp(argv[i], "--lim=", 6) == 0) {
				args->queue_memory_limit = atoi(argv[i] + 6) * 1024 * 1024;
			} else if (strncmp(argv[i], "--output=", 9) == 0) {
				args->output_filename = argv[i] + 9; // make save with template
			} else if (strncmp(argv[i], "--rww=", 6) == 0) {
				rww_values = argv[i] + 6;
				parsed_count = sscanf(rww_values, "%d,%d,%d", &ret_temp, &wot_temp, &wrt_temp);

				if (parsed_count != 3) {
					fprintf(stderr, "Error: Invalid format for --rww. Expected --rww=W,R,T (e.g., --rww=2,1,4). Got: %s\n", rww_values);
					return -1;
				}
				if (wrt_temp <= 0 || wrt_temp > UCHAR_MAX || ret_temp <= 0 || ret_temp > UCHAR_MAX || wot_temp <= 0 || wot_temp > UCHAR_MAX) {
					fprintf(stderr, "Error: Thread counts in --rww must be between 0 and %d. Got: W=%d, R=%d, T=%d\n", UCHAR_MAX, wrt_temp, ret_temp, wot_temp);
					return -1;
				}

				args->wrt_count = (uint8_t)wrt_temp;
				args->ret_count = (uint8_t)ret_temp;
				args->wot_count = (uint8_t)wot_temp;
				rww_found = 1;
				argv[i] = "_";
			} else if (strncmp(argv[i], "_", 1) == 0) {
				continue;

			} else if (args->input_filename[args->file_count] == NULL) {
				if (args->file_count == MAX_IMAGE_QUEUE_SIZE)
					continue;
				args->input_filename[args->file_count] = argv[i];
				args->file_count++;
			} else {
				fprintf(stderr, "Error: Unknown argument %s\n", argv[i]);
				return -1;
			}
		}
		if ((args->ret_count + args->wot_count + args->wrt_count) < 3) {
			fprintf(stderr, "Error: queue-based mode requires more than 3 threads all in all. see README\n");
		}
		if (!rww_found) {
			fprintf(stderr, "Error: queue-based mode requires --rww=W,R,T argument.\n");
			return -1;
		}

	} else {
		for (int i = 1; i < argc; i++) {
			//			printf("%d arg: %s\n", i, argv[i]);
			if (strncmp(argv[i], "--threadnum=", 12) == 0) {
				args->threadnum = atoi(argv[i] + 12);
				if (args->threadnum <= 0) {
					fprintf(stderr, "Error: Invalid threadnum.\n");
					return -1;
				}
			} else if (strncmp(argv[i], "--log=", 6) == 0) {
				args->log_enabled = atoi(argv[i] + 6);
			} else if (strncmp(argv[i], "--output=", 9) == 0) {
				args->output_filename = argv[i] + 9;
				printf("1: %s\n", argv[i] + 9);
			} else if (args->input_filename[0] == NULL && strncmp(argv[i], "_", 1)) {
				args->input_filename[0] = argv[i];
				args->file_count++;
				printf("input filename %s\n", argv[i]);
			} else if (strncmp(argv[i], "_", 1) == 0) {
				continue;
			} else {
				fprintf(stderr, "Error: Unknown argument %s\n", argv[i]);
				return -1;
			}
		}
		if (args->file_count != 1) {
			fprintf(stderr, "Error: not queued mode requires strictly 1 input image\n");
			return -1;
		}
	}

	if (!args->input_filename[0] || !args->filter_type || args->compute_mode == -1 || args->block_size == 0) {
		fprintf(stderr, "Error: Missing required arguments.\n");
		return -1;
	}

	return args->threadnum;
}

static void *sthread_function(void *arg)
{
	struct thread_spec *th_spec = (struct thread_spec *)arg;
	int result = 0;

	while (1) {
		switch ((enum compute_mode)args->compute_mode) {
		case BY_ROW:
			result = process_by_row(th_spec, &next_x_block, args->block_size, &xy_block_mutex);
			break;
		case BY_COLUMN:
			result = process_by_column(th_spec, &next_y_block, args->block_size, &xy_block_mutex);
			break;
		case BY_PIXEL:
			result = process_by_pixel(th_spec, &next_x_block, &next_y_block, &xy_block_mutex);
			break;
		case BY_GRID:
			result = process_by_grid(th_spec, &next_x_block, &next_y_block, args->block_size, &xy_block_mutex);
			break;
		default:
			fprintf(stderr, "Error: Invalid mode %d in thread function.\n", args->compute_mode);
			result = 1;
			break;
		}

		if (result != 0) {
			goto exit;
		}
		if (!th_spec || !args || !args->filter_type || !filters) {
			fprintf(stderr, "Error: Invalid state before filter_part_computation.\n");
			return NULL;
		}

		filter_part_computation(th_spec, args->filter_type, filters);
	}

exit:
	free(th_spec);
	return NULL;
}

static void sthreads_save(char *output_filepath, size_t path_len, int threadnum, bmp_img *img_result)
{
	if (strcmp(args->output_filename, "") != 0) {
		snprintf(output_filepath, path_len, "test-img/%s", args->output_filename);
	} else {
		if (threadnum > 1)
			snprintf(output_filepath, path_len, "test-img/rcon_out_%s", args->input_filename[0]);
		else
			snprintf(output_filepath, path_len, "test-img/seq_out_%s", args->input_filename[0]);
	}

	printf("result out filepath %s\n", output_filepath);
	bmp_img_write(img_result, output_filepath);
}

static double execute_sthreads(int threadnum, struct img_dim *dim, struct img_spec *img_spec)
{
	assert(threadnum > 0);

	pthread_t *th = NULL;
	double start_time = 0, end_time = 0;
	int create_error = 0;
	struct thread_spec *th_spec[threadnum];

	if (threadnum > 1) {
		th = malloc(threadnum * sizeof(pthread_t));
		if (!th) {
			free(th);
			goto mem_err;
		}
		// setup threadlocalc details before thread creation
		for (int i = 0; i < threadnum; i++) {
			th_spec[i] = thread_spec_init();
			if (!th_spec[i]) {
				create_error = 1;
				fprintf(stderr, "Memory allocation error for thread_spec %d\n", i);
				threadnum = i;
				break;
			}

			th_spec[i]->dim = dim;
			th_spec[i]->img = img_spec;
		}

		start_time = get_time_in_seconds();
		for (int i = 0; i < threadnum; i++) {
			if (pthread_create(&th[i], NULL, sthread_function, th_spec[i]) != 0) {
				perror("Failed to create a thread");
				free(th_spec[i]);
				create_error = 1;
				threadnum = i;
				break;
			}
		}

		for (int i = 0; i < threadnum; i++) {
			if (pthread_join(th[i], NULL)) {
				perror("Failed to join a thread");
				break;
			}
		}

		end_time = get_time_in_seconds();

		if (create_error) {
			free(th);
			return 0;
		}

		free(th);
	} else if (threadnum == 1) {
		start_time = get_time_in_seconds();

		th_spec[0] = thread_spec_init();
		if (!th_spec[0]) {
			free(th_spec[0]);
			goto mem_err;
		}

		th_spec[0]->dim = dim;
		th_spec[0]->img = img_spec;
		th_spec[0]->end_column = th_spec[0]->dim->width;
		th_spec[0]->end_row = th_spec[0]->dim->height;
		filter_part_computation(th_spec[0], args->filter_type, filters);
		end_time = get_time_in_seconds();

		free(th_spec[0]);
	} else { // unreachable
		fprintf(stderr, "Error: Invalid thread count in execute_sthreads.\n");
		return 0;
	}

	return end_time - start_time;

mem_err:
	fprintf(stderr, "Memory allocation error\n");
	return 0;
}

static double execute_qthreads(void)
{
	double start_time, end_time = 0;
	struct img_queue input_queue, output_queue;
	struct qthreads_info *qt_info = NULL;

	qt_info = malloc(sizeof(struct qthreads_info));
	if (!qt_info)
		goto mem_err;

	if (allocate_qthread_resources(qt_info, args, &input_queue, &output_queue) != 0) {
		return 0;
	}
	qt_info->filters = filters;

	start_time = get_time_in_seconds();

	create_qthreads(qt_info, args);

	join_qthreads(qt_info);

	end_time = get_time_in_seconds();

	free_qthread_resources(qt_info);

	return end_time - start_time;

mem_err:
	fprintf(stderr, "Error: memory allocation failed at allocate_qthread_resources\n");
	return 0;
}

int main(int argc, char *argv[])
{
	bmp_img img, img_result;
	pthread_t *th = NULL;
	struct img_dim *dim = NULL;
	struct img_spec *img_spec = NULL;
	char input_filepath[MAX_PATH_LEN], output_filepath[MAX_PATH_LEN];
	double result_time = 0;
	int threadnum = 0;

	args = malloc(sizeof(struct p_args));

	threadnum = parse_args(argc, argv);
	if (threadnum < 0) {
		fprintf(stderr, "Error: couldn't parse the args\n");
		return -1;
	}

	if (!args->queue_mode) { // move to sep file
		if (threadnum > 1) {
			th = malloc(threadnum * sizeof(pthread_t));
			if (!th) {
				free(th);
				goto mem_err;
			}
		}
		snprintf(input_filepath, sizeof(input_filepath), "test-img/%s", args->input_filename[0]);
		if (bmp_img_read(&img, input_filepath)) {
			fprintf(stderr, "Error: Could not open BMP image\n");
			return -1;
		}

		dim = init_dimensions(img.img_header.biWidth, img.img_header.biHeight);
		if (!dim) {
			bmp_img_free(&img);
			goto mem_err;
		}

		bmp_img_init_df(&img_result, dim->width, dim->height);
		img_spec = init_img_spec(&img, &img_result);
	}

	filters = malloc(sizeof(struct filter_mix));
	if (!filters) {
		free(th);
		free(filters);
		goto mem_err;
	}
	init_filters(filters);

	result_time = (!args->queue_mode) ? execute_sthreads(threadnum, dim, img_spec) : execute_qthreads();
	st_write_logs(args, result_time);

	if (!args->queue_mode) {
		sthreads_save(output_filepath, sizeof(output_filepath), threadnum, &img_result);
		free(th);
		bmp_img_free(&img_result);
		free(dim);
		bmp_img_free(&img);
	}

	free_filters(filters);
	pthread_mutex_destroy(&xy_block_mutex);
	return 0;

mem_err:
	fprintf(stderr, "Memory allocation error\n");
	free(args);
	return -1;
}
