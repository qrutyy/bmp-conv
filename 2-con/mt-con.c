// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include "../utils/utils.h"
#include "../utils/mt-utils.h"
#include "../libbmp/libbmp.h"

/* *Real* mode (columns/rows per thread == const) */

#define LOG_FILE_PATH "tests/timing-results.dat"

const char *input_filename;
const char *output_filename = NULL;
const char *filter_type;
const char *mode_str;
enum compute_mode mode;
struct filter_mix *filters = NULL;

int block_size = 0;
int next_x_block = 0;
int next_y_block = 0;
pthread_mutex_t xy_block_mutex = PTHREAD_MUTEX_INITIALIZER;

int parse_args(int argc, char *argv[])
{

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

    for (int i = 6; i < argc; i++) {
        if (strncmp(argv[i], "--output=", 9) == 0) {
            output_filename = argv[i] + 9;
            printf("Output filename set to: %s\n", output_filename);
        }
    }

	return threadnum;
}

void filter_part_computation(struct thread_spec *spec)
{
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
	} else {
		fprintf(stderr, "Wrong filter type parameter\n");
	}
}

void *thread_function(void *arg)
{
	struct thread_spec *th_spec = (struct thread_spec *)arg;

	while (1) {
		switch (mode) {
		case BY_ROW:
			if (process_by_row(th_spec, &next_x_block, block_size, &xy_block_mutex))
					goto exit;
			break;
		case BY_COLUMN:
				if (process_by_column(th_spec, &next_y_block, block_size, &xy_block_mutex))
					goto exit;
			break;
		case BY_PIXEL:
				if (process_by_pixel(th_spec, &next_x_block, &next_y_block, &xy_block_mutex))
					goto exit;
			break;
		case BY_GRID:
				if (process_by_grid(th_spec, &next_x_block, &next_y_block, block_size, &xy_block_mutex))
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
	FILE *file = NULL;

	threadnum = parse_args(argc, argv);
	if (threadnum < 0) {
		fprintf(stderr, "Error: couldn't parse the args\n");
		return -1;
	}
	th = malloc(threadnum * sizeof(pthread_t));
	if (!th)
		goto mem_err;

	snprintf(input_filepath, sizeof(input_filepath), "../test-img/%s", input_filename);

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
	if (filters) {
		free(filters);
		goto mem_err;
	}
	init_filters(filters);

	start_time = get_time_in_seconds();

	for (i = 0; i < 3; i++) {
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

	printf("RESULT: filter = %s, threadnum = %d, time = %.6f seconds\n\n", filter_type, threadnum,
	       end_time - start_time);

	file = fopen(LOG_FILE_PATH, "a");
    if (file) {
		fprintf(file, "%s %d %s %d %.6f\n", filter_type, threadnum, mode_str, block_size, end_time - start_time);
        fclose(file);
    } else {
        fprintf(stderr, "Error: could not open timing results file\n");
    }

	if (output_filename) {
		snprintf(output_filepath, sizeof(output_filepath), "../test-img/%s", output_filename);
	} else {
		snprintf(output_filepath, sizeof(output_filepath), "../test-img/rcon_out_%s", input_filename);
	}

	bmp_img_write(&img_result, output_filepath);

	free(th);
	free(dim);
	bmp_img_free(&img);
	bmp_img_free(&img_result);
	pthread_mutex_destroy(&xy_block_mutex);
	return 0;

mem_err:
	fprintf(stderr, "Memory allocation error\n");
	free(th);
	free(dim);
	return -1;
}
