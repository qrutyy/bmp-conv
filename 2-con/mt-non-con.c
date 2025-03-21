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

/** Not concurrent multithreaded mode (columns/rows per thread != const)
  * It just divides the matrix into threadnum equal parts
  * For concurrent -> see real-con.c
  */

const char *input_filename;
const char *filter_type;
struct filter blur, motion_blur, gaus_blur, conv, sharpen, embos;

int parse_args(int argc, char *argv[], int *threadnum, enum compute_mode *mode)
{
	const char *mode_str;

	if (argc < 4) {
		printf("Usage: %s <input_image> <filter_type>\n", argv[0]);
		return -1;
	}

	input_filename = argv[1];
	filter_type = argv[2];

	printf("Input image: %s\n", input_filename);
	printf("Filter type: %s\n", filter_type);

	if (strncmp(argv[3], "--threadnum=", 12) == 0) {
		*threadnum = atoi(argv[3] + 12);
		printf("Number of threads: %d\n", *threadnum);
	} else {
		fprintf(stderr, "Please use correct arg descriptors\n");
		return -1;
	}

	if (strncmp(argv[4], "--mode=", 7) == 0) {
		mode_str = argv[4] + 7;

		if (strcmp(mode_str, "by_row") == 0) {
			*mode = BY_ROW;
		} else if (strcmp(mode_str, "by_column") == 0) {
			*mode = BY_COLUMN;
		} else if (strcmp(mode_str, "by_pixel") == 0) {
			*mode = BY_PIXEL;
		} else if (strcmp(mode_str, "by_grid") == 0) {
			*mode = BY_GRID;
		} else {
			fprintf(stderr, "Error: Invalid mode. Use by_row, by_column, by_pixel, or by_grid\n");
			return -1;
		}
		printf("Mode selected: %s\n\n", mode_str);
	} else {
		fprintf(stderr, "Error: Missing or incorrect --mode argument\n");
		return -1;
	}

	return 0;
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

void *thread_function(void *arg)
{
	struct thread_spec *spec = (struct thread_spec *)arg;
	filter_part_computation(spec);
	free(spec);
	return NULL;
}

void *thread_spec_init(bmp_img *img, int i, int threadnum, enum compute_mode m)
{
	int count = 0;
	struct thread_spec *th_spec = malloc(sizeof(struct thread_spec));
	if (!th_spec)
		return NULL;

	switch (m) {
	case BY_ROW:
		count = img->img_header.biHeight / threadnum;
		th_spec->start_row = i * count;
		th_spec->start_column = 0;
		th_spec->end_row = th_spec->start_row + count;
		th_spec->end_column = img->img_header.biWidth;
		break;
	case BY_COLUMN:
		count = img->img_header.biWidth / threadnum;
		th_spec->start_column = i * count;
		th_spec->start_row = 0;
		th_spec->end_row = img->img_header.biHeight;
		th_spec->end_column = th_spec->start_column + count;
		break;
	case BY_PIXEL:
		fprintf(stderr, "Error: Not implemented yet and wouldn't be. Check con version.\n");
		return NULL;
	case BY_GRID:
		fprintf(stderr, "Error: Not implemented yet and wouldn't be. Check con version.\n");
		return NULL;
	}
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
	int i = 0, threadnum;
	enum compute_mode mode;

	parse_args(argc, argv, &threadnum, &mode);
	th = malloc(threadnum * sizeof(pthread_t));
	if (!th)
		goto mem_err;

	snprintf(input_filepath, sizeof(input_filepath), "../test/%s", input_filename);

	if (bmp_img_read(&img, input_filepath)) {
		fprintf(stderr, "Error: Could not open BMP image\n");
		return -1;
	}
	if (img.img_header.biWidth % threadnum != 0) {
		fprintf(stderr,
			"Warning: threadnum should divide BMP width with no remainder if non-constant mode is selected\n");
		do {
			threadnum--;
		} while (img.img_header.biWidth % threadnum == 0);

		fprintf(stderr, "Warning: decreased threadnum to %d\n", threadnum);
		if (threadnum <= 1) {
			fprintf(stderr, "Error: no dividers of biWidth found. Try bigger value\n");
			return -1;
		}
	}

	dim = init_dimensions(img.img_header.biWidth, img.img_header.biHeight);
	if (!dim)
		goto mem_err;

	bmp_img_init_df(&img_result, dim->width, dim->height);
	img_spec = init_img_spec(&img, &img_result);
	init_filters(&blur, &motion_blur, &gaus_blur, &conv, &sharpen, &embos);

	start_time = get_time_in_seconds();

	for (i = 0; i < threadnum; i++) {
		th_spec = thread_spec_init(&img, i, threadnum, mode);
		if (!th_spec) {
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

	printf("RESULT: filter = %s, threadnum = %d, time = %.6f seconds\n", filter_type, threadnum,
	       end_time - start_time);
	snprintf(output_filepath, sizeof(output_filepath), "../test/con_out_%s", input_filename);

	bmp_img_write(&img_result, output_filepath);
	compare_images(&img, &img_result);

	free(th);
	free(dim);
	bmp_img_free(&img);
	bmp_img_free(&img_result);
	return 0;

mem_err:
	fprintf(stderr, "Memory allocation error\n");
	free(th);
	free(dim);
	return -1;
}
