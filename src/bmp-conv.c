// SPDX-License-Identifier: GPL-3.0-or-later

#include "../libbmp/libbmp.h"
#include "utils/mt-utils.h"
#include "utils/utils.h"
#include <math.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "bmp-conv.h"

struct filter_mix *filters = NULL;
struct p_args *args = NULL;

int next_x_block = 0;
int next_y_block = 0;
pthread_mutex_t xy_block_mutex = PTHREAD_MUTEX_INITIALIZER;

const char *valid_filters[] = { "bb", "mb", "em", "gg", "gb", "co", "sh", "mm", "bo", "mg", NULL };
const char *valid_modes[] = { "by_row", "by_column", "by_pixel", "by_grid", NULL };

char *check_filter_arg(char *filter)
{
	for (int i = 0; valid_filters[i] != NULL; i++) {
		if (strcmp(filter, valid_filters[i]) == 0) {
			return filter;
		}
	}
	fprintf(stderr, "Error: Wrong filter.\n");
	return NULL;
}

enum compute_mode check_mode_arg(char *mode_str)
{
	for (int i = 0; valid_modes[i] != NULL; i++) {
		if (strcmp(mode_str, valid_modes[i]) == 0) {
			return i;
		}
	}
	fprintf(stderr, "Error: Invalid mode.\n");
	return -1;
}

const char *mode_to_str(enum compute_mode mode)
{
	if (mode >= 0 && mode < (sizeof(valid_modes) / sizeof(valid_modes[0]) - 1)) {
		return valid_modes[mode];
	}
	return "unknown";
}

int parse_args(int argc, char *argv[], struct p_args *args)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <input_file.bmp> --mode=<compute_mode> --filter=<type>  --threadnum=<N> --block=<size> [--output=<file>]\n", argv[0]);
		return -1;
	}

	args->threadnum = 1;
	args->block_size = 0;
	args->input_filename = NULL;
	args->output_filename = NULL;
	args->filter_type = NULL;
	args->mode = -1;
	args->log_enabled = 0;

	for (int i = 1; i < argc; i++) {
		if (strncmp(argv[i], "--filter=", 9) == 0) {
			args->filter_type = check_filter_arg(argv[i] + 9);
			if (!args->filter_type)
				return -1;
		} else if (strncmp(argv[i], "--mode=", 7) == 0) {
			args->mode = check_mode_arg(argv[i] + 7);
			if (args->mode < 0)
				return -1;
		} else if (strncmp(argv[i], "--threadnum=", 12) == 0) {
			args->threadnum = atoi(argv[i] + 12);
			if (args->threadnum <= 0) {
				fprintf(stderr, "Error: Invalid threadnum.\n");
				return -1;
			}
		} else if (strncmp(argv[i], "--block=", 8) == 0) {
			args->block_size = atoi(argv[i] + 8);
			if (args->block_size <= 0) {
				fprintf(stderr, "Error: Block size must be >= 1.\n");
				return -1;
			}
		} else if (strncmp(argv[i], "--log=", 6) == 0) {
			args->log_enabled = atoi(argv[i] + 6);
		} else if (strncmp(argv[i], "--output=", 9) == 0) {
			args->output_filename = argv[i] + 9;
		} else if (args->input_filename == NULL) {
			args->input_filename = argv[i]; // Assume first positional argument is input image
		} else {
			fprintf(stderr, "Error: Unknown argument %s\n", argv[i]);
			return -1;
		}
	}

	if (!args->input_filename || !args->filter_type || args->mode == -1 || args->block_size == 0) {
		fprintf(stderr, "Error: Missing required arguments.\n");
		return -1;
	}

	return args->threadnum;
}

void filter_part_computation(struct thread_spec *spec)
{
	char *filter_type = args->filter_type;
	if (strcmp(filter_type, "mb") == 0) {
		apply_filter(spec, *filters->motion_blur);
	} else if (strcmp(filter_type, "bb") == 0) {
		apply_filter(spec, *filters->blur);
	} else if (strcmp(filter_type, "gb") == 0) {
		apply_filter(spec, *filters->gaus_blur);
	} else if (strcmp(filter_type, "co") == 0) {
		apply_filter(spec, *filters->conv);
	} else if (strcmp(filter_type, "sh") == 0) {
		apply_filter(spec, *filters->sharpen);
	} else if (strcmp(filter_type, "em") == 0) {
		apply_filter(spec, *filters->emboss);
	} else if (strcmp(filter_type, "mm") == 0) {
		apply_median_filter(spec, 15);
	} else if (strcmp(filter_type, "gg") == 0) {
		apply_filter(spec, *filters->big_gaus);
	} else if (strcmp(filter_type, "bo") == 0) {
		apply_filter(spec, *filters->box_blur);
	} else if (strcmp(filter_type, "mg") == 0) {
		apply_filter(spec, *filters->med_gaus);
	} else {
		fprintf(stderr, "Wrong filter type parameter\n");
	}
}

void *thread_function(void *arg)
{
	struct thread_spec *th_spec = (struct thread_spec *)arg;

	while (1) {
		switch (args->mode) {
		case BY_ROW:
			if (process_by_row(th_spec, &next_x_block, args->block_size, &xy_block_mutex))
				goto exit;
			break;
		case BY_COLUMN:
			if (process_by_column(th_spec, &next_y_block, args->block_size, &xy_block_mutex))
				goto exit;
			break;
		case BY_PIXEL:
			if (process_by_pixel(th_spec, &next_x_block, &next_y_block, &xy_block_mutex))
				goto exit;
			break;
		case BY_GRID:
			if (process_by_grid(th_spec, &next_x_block, &next_y_block, args->block_size, &xy_block_mutex))
				goto exit;
			break;
		default:
			free(th_spec);
			return NULL;
		}

		filter_part_computation(th_spec);
	}

exit:
	free(th_spec);
	return NULL;
}

double execute_threads(int threadnum, struct img_dim *dim, struct img_spec *img_spec)
{
	struct thread_spec *th_spec = NULL;
	pthread_t *th = NULL;
	double start_time, end_time;

	if (threadnum > 1) {
		th = malloc(threadnum * sizeof(pthread_t));
		if (!th)
			goto mem_err;

		start_time = get_time_in_seconds();

		for (int i = 0; i < threadnum; i++) {
			th_spec = thread_spec_init();
			if (!th_spec)
				goto mem_err;

			th_spec->dim = dim;
			th_spec->img = img_spec;

			if (pthread_create(&th[i], NULL, thread_function, th_spec) != 0) {
				perror("Failed to create a thread");
				free(th_spec);
			}
		}

		for (int i = 0; i < threadnum; i++)
			pthread_join(th[i], NULL);

		end_time = get_time_in_seconds();
		free(th);
	} else {
		// case when threadnum < 0 already was checked in parse_args
		start_time = get_time_in_seconds();

		th_spec = thread_spec_init();
		if (!th_spec)
			goto mem_err;

		th_spec->dim = dim;
		th_spec->img = img_spec;
		th_spec->end_column = th_spec->dim->width;
		th_spec->end_row = th_spec->dim->height;
		filter_part_computation(th_spec);
		end_time = get_time_in_seconds();

		free(th_spec);
	}

	return end_time - start_time;

mem_err:
	fprintf(stderr, "Memory allocation error\n");
	free(th);
	free(th_spec);
	return 0;
}

void write_logs(struct p_args *args, FILE *file, double result_time)
{
	if (!args->log_enabled)
		return;

	file = fopen(LOG_FILE_PATH, "a");
	const char *mode_str = (args->threadnum == 1) ? "none" : mode_to_str(args->mode);

	if (file) {
		fprintf(file, "%s %d %s %d %.3f\n", args->filter_type, args->threadnum, mode_str, args->block_size, result_time);
		fclose(file);
	} else {
		fprintf(stderr, "Error: could not open timing results file\n");
	}

	printf("RESULT: filter = %s, threadnum = %d, time = %.6f seconds\n\n", args->filter_type, args->threadnum, result_time);
	return;
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
	FILE *file = NULL;

	args = malloc(sizeof(struct p_args));
	if (!args) {
		goto mem_err;
	}

	threadnum = parse_args(argc, argv, args);
	if (threadnum < 0) {
		fprintf(stderr, "Error: couldn't parse the args\n");
		return -1;
	}

	if (threadnum > 1) {
		th = malloc(threadnum * sizeof(pthread_t));
		if (!th) {
			free(th);
			goto mem_err;
		}
	}

	snprintf(input_filepath, sizeof(input_filepath), "./test-img/%s", args->input_filename);

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

	filters = malloc(sizeof(struct filter_mix));
	if (!filters) {
		free(filters);
		goto mem_err;
	}

	init_filters(filters);

	result_time = execute_threads(threadnum, dim, img_spec);
	write_logs(args, file, result_time);
	printf("outf%sf\n", args->output_filename);
	if (strcmp(args->output_filename, "") != 0) {
		snprintf(output_filepath, sizeof(output_filepath), "test-img/%s", args->output_filename);
	} else {
		if (threadnum > 1)
			snprintf(output_filepath, sizeof(output_filepath), "test-img/rcon_out_%s", args->input_filename);
		else
			snprintf(output_filepath, sizeof(output_filepath), "test-img/seq_out_%s", args->input_filename);
	}
	printf("result out filepath %s\n", output_filepath);
	bmp_img_write(&img_result, output_filepath);

	free(th);
	free(dim);
	bmp_img_free(&img);
	bmp_img_free(&img_result);
	free_filters(filters);
	pthread_mutex_destroy(&xy_block_mutex);
	return 0;

mem_err:
	fprintf(stderr, "Memory allocation error\n");
	free(args);
	return -1;
}
