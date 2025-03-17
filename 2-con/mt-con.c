// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include "../libbmp/libbmp.h"
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "mt-utils.h"
#include <pthread.h>
#include <sys/time.h>
#include "../utils/utils.h"

/* *Real* mode (columns/rows per thread == const) */

const char *input_filename;
const char *filter_type;
struct filter blur, motion_blur, gaus_blur, conv, sharpen, embos;
int block_size = 0;
int next_x_block = 0;
int next_y_block = 0;
pthread_mutex_t x_block_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t y_block_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t xy_block_mutex = PTHREAD_MUTEX_INITIALIZER;
enum compute_mode mode;

int parse_args(int argc, char *argv[])
{
	const char *mode_str;
	int threadnum = 0;

	if (argc < 4) {
		printf("Usage: %s <input_image> <filter_type>\n", argv[0]);
		return -1;
	}

	input_filename = argv[1];
	filter_type = argv[2];

	printf("Input image: %s\n", input_filename);
	printf("Filter type: %s\n", filter_type);

	if (strncmp(argv[3], "--threadnum=", 12) == 0) {
		threadnum = atoi(argv[3] + 12);
		printf("Number of threads: %d\n", threadnum);
	} else {
		fprintf(stderr, "Please use correct arg descriptors\n");
		return -1;
	}

	if (strncmp(argv[4], "--mode=", 7) == 0) {
		mode_str = argv[4] + 7;

		if (strcmp(mode_str, "by_row") == 0) {
			mode = BY_ROW;
		} else if (strcmp(mode_str, "by_column") == 0) {
			mode = BY_COLUMN;
		} else if (strcmp(mode_str, "by_pixel") == 0) {
			mode = BY_PIXEL;
		} else if (strcmp(mode_str, "by_grid") == 0) {
			mode = BY_GRID;
		} else {
			fprintf(stderr, "Error: Invalid mode. Use by_row, by_column, by_pixel, or by_grid\n");
			return -1;
		}
		printf("Mode selected: %s\n\n", mode_str);
	} else {
		fprintf(stderr, "Error: Missing or incorrect --mode argument\n");
		return -1;
	}

	if (strncmp(argv[5], "--block=", 8) == 0) {
		block_size = atoi(argv[5] + 8);
		if (block_size <= 0) {
			fprintf(stderr, "Error: Block size cant be smaller then 1\n");
			return -1;
		}
	} else {
		fprintf(stderr, "Error: Missing or incorrect --block= argument\n");
		return -1;
	}

	return threadnum;
}

void filter_part_computation(struct thread_spec *spec)
{
	if (strcmp(filter_type, "mb") == 0) {
		apply_filter(spec, motion_blur);
	} else if (strcmp(filter_type, "bb") == 0) {
		apply_filter(spec, blur);
	} else if (strcmp(filter_type, "gb") == 0) {
		apply_filter(spec, gaus_blur);
	} else if (strcmp(filter_type, "co") == 0) {
		apply_filter(spec, conv);
	} else if (strcmp(filter_type, "sh") == 0) {
		apply_filter(spec, sharpen);
	} else if (strcmp(filter_type, "em") == 0) {
		apply_filter(spec, embos);
	} else if (strcmp(filter_type, "mm") == 0) {
		apply_median_filter(spec, 15);
	} else {
		fprintf(stderr, "Wrong filter type parameter\n");
	}
}

void process_by_row(struct thread_spec *th_spec)
{
	pthread_mutex_lock(&x_block_mutex);
	printf("next_block: %d, height: %d\n", next_x_block, th_spec->dim->height);

	if (next_x_block >= th_spec->dim->height) {
		pthread_mutex_unlock(&x_block_mutex);
		return;
	}
	th_spec->start_row = next_x_block;
	next_x_block += block_size;
	pthread_mutex_unlock(&x_block_mutex);

	th_spec->start_column = 0;
	th_spec->end_row = fmin(th_spec->start_row + block_size, th_spec->dim->height);
	th_spec->end_column = th_spec->dim->width;
}

void process_by_column(struct thread_spec *th_spec)
{
	pthread_mutex_lock(&y_block_mutex);
	printf("next_block: %d, width: %d\n", next_y_block, th_spec->dim->width);

	if (next_y_block >= th_spec->dim->width) {
		pthread_mutex_unlock(&y_block_mutex);
		return;
	}

	th_spec->start_column = next_y_block;
	next_y_block += block_size;
	pthread_mutex_unlock(&y_block_mutex);

	th_spec->start_row = 0;
	th_spec->end_row = th_spec->dim->height;
	th_spec->end_column = fmin(th_spec->start_column + block_size, th_spec->dim->width);
}

void process_by_pixel(struct thread_spec *th_spec)
{
	pthread_mutex_lock(&xy_block_mutex);
	if (next_x_block >= th_spec->dim->height || next_y_block >= th_spec->dim->width) {
		pthread_mutex_unlock(&xy_block_mutex);
		return;
	}

	th_spec->start_row = next_x_block;
	th_spec->start_column = next_y_block;
	next_y_block += 1;

	if (next_y_block >= th_spec->dim->width) {
		next_y_block = 0;
		next_x_block += 1;
	}
	pthread_mutex_unlock(&xy_block_mutex);

	th_spec->end_row = th_spec->start_row + 1;
	th_spec->end_column = th_spec->start_column + 1;
	printf("Row: st: %d, end: %d, Column: st: %d, end: %d \n", th_spec->start_row, th_spec->end_row,
	       th_spec->start_column, th_spec->end_column);
}

void process_by_grid(struct thread_spec *th_spec)
{
	pthread_mutex_lock(&xy_block_mutex);
	if (next_x_block >= th_spec->dim->height || next_y_block >= th_spec->dim->width) {
		pthread_mutex_unlock(&xy_block_mutex);
		return;
	}

	th_spec->start_row = next_x_block;
	th_spec->start_column = next_y_block;
	next_y_block += block_size;

	if (next_y_block >= th_spec->dim->width) {
		next_y_block = 0;
		next_x_block += block_size;
	}
	pthread_mutex_unlock(&xy_block_mutex);

	th_spec->end_row = fmin(th_spec->start_row + block_size, th_spec->dim->height);
	th_spec->end_column = fmin(th_spec->start_column + block_size, th_spec->dim->width);
	printf("Row: st: %d, end: %d, Column: st: %d, end: %d \n", th_spec->start_row, th_spec->end_row,
	       th_spec->start_column, th_spec->end_column);
}

void *thread_function(void *arg)
{
	struct thread_spec *th_spec = (struct thread_spec *)arg;

	while (1) {
		switch (mode) {
		case BY_ROW:
			process_by_row(th_spec);
			break;
		case BY_COLUMN:
			process_by_column(th_spec);
			break;
		case BY_PIXEL:
			process_by_pixel(th_spec);
			break;
		case BY_GRID:
			process_by_grid(th_spec);
			break;
		default:
			free(th_spec);
			return NULL;
		}

		filter_part_computation(th_spec);
	}

	free(th_spec);
	return NULL;
}

void *thread_spec_init()
{
	struct thread_spec *th_spec = malloc(sizeof(struct thread_spec));
	if (!th_spec)
		return NULL;

	return th_spec;
}

int main(int argc, char *argv[])
{
	bmp_img img, img_result;
	pthread_t *th = NULL;
	struct img_dim *dim = NULL;
	struct img_spec *img_spec = NULL;
	struct thread_spec *th_spec = NULL;
	char input_filepath[MAX_PATH_LEN], output_filepath[MAX_PATH_LEN];
	double start_time, end_time = 0;
	int i, threadnum = 0;

	threadnum = parse_args(argc, argv);
	if (threadnum < 0) {
		fprintf(stderr, "Error: couldn't parse the args\n");
		return -1;
	}
	th = malloc(threadnum * sizeof(pthread_t));
	if (!th)
		goto mem_err;

	snprintf(input_filepath, sizeof(input_filepath), "../test/%s", input_filename);

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
	init_filters(&blur, &motion_blur, &gaus_blur, &conv, &sharpen, &embos);

	start_time = get_time_in_seconds();

	for (i = 0; i < threadnum; i++) {
		th_spec = thread_spec_init();
		if (!th_spec) {
			bmp_img_free(&img);
			bmp_img_free(&img_result);
			goto mem_err;
		}
		th_spec->dim = dim;
		th_spec->img = img_spec;

		if (pthread_create(&th[i], NULL, thread_function, th_spec) != 0) {
			perror("Failed to create a thread");
			free(th_spec);
		}
	}

	for (i = 0; i < threadnum; i++)
		pthread_join(th[i], NULL);

	end_time = get_time_in_seconds();

	printf("\nRESULT: filter = %s, threadnum = %d, time = %.6f seconds\n", filter_type, threadnum,
	       end_time - start_time);
	snprintf(output_filepath, sizeof(output_filepath), "../test/rcon_out_%s", input_filename);

	bmp_img_write(&img_result, output_filepath);
	compare_images(&img, &img_result);

	free(th);
	free(dim);
	bmp_img_free(&img);
	bmp_img_free(&img_result);
	pthread_mutex_destroy(&x_block_mutex);
	pthread_mutex_destroy(&y_block_mutex);
	pthread_mutex_destroy(&xy_block_mutex);
	return 0;

mem_err:
	fprintf(stderr, "Memory allocation error\n");
	free(th);
	free(dim);
	return -1;
}
