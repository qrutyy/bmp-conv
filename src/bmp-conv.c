// SPDX-License-Identifier: GPL-3.0-or-later

#include "../libbmp/libbmp.h"
#include "utils/utils.h"
#include "mt-mode/exec.h"
#include "st-mode/exec.h"
#include "qmt-mode/exec.h"
#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>

#define LOG_FILE_PATH "tests/timing-results.dat"

struct p_args *args = NULL;

static int parse_args(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <input_file.bmp> --mode=<compute_mode> --filter=<type> --threadnum=<N> --block=<size> [--output=<file>]\n", argv[0]);
		return -1;
	}

	initialize_args(args);

	if (strncmp(argv[1], "-queue-mode", 6) == 0)
		args->queue_mode = 1;

	if (parse_mandatory_args(argc, argv, args) < 0)
		return -1;

	if (args->queue_mode) {
		if (parse_queue_mode_args(argc, argv, args) < 0)
			return -1;
	} else {
		if (parse_normal_mode_args(argc, argv, args) < 0)
			return -1;
	}

	if (!args->input_filename[0] || !args->filter_type || args->compute_mode == -1 || args->block_size == 0) {
		fprintf(stderr, "Error: Missing required arguments.\n");
		return -1;
	}

	return args->threadnum;
}

static int run_non_queue_mode(int threadnum, struct filter_mix *filters) {
    struct bmp_image img = {0};
    struct bmp_image img_result = {0};
    struct img_dim *dim = NULL;
    struct img_spec *img_spec = NULL;
    char input_filepath[256];
    char output_filepath[256];
    double result_time = 0;
    int status = -1;

    snprintf(input_filepath, sizeof(input_filepath), "test-img/%s", args->input_filename[0]);

    if (bmp_img_read(&img, input_filepath)) {
        fprintf(stderr, "Error: Could not open BMP image '%s'\n", input_filepath);
        goto cleanup;
    }

    dim = init_dimensions(img.img_header.biWidth, img.img_header.biHeight);
    if (!dim) {
        fprintf(stderr, "Error: Failed to initialize dimensions.\n");
        goto cleanup;
    }

    bmp_img_init_df(&img_result, dim->width, dim->height);

    img_spec = init_img_spec(&img, &img_result);
     if (!img_spec) {
         fprintf(stderr, "Error: Failed to initialize image spec.\n");
         goto cleanup;
    }

    assert(threadnum > 0);

    if (threadnum > 1) {
        result_time = execute_mt_computation(threadnum, dim, img_spec, args, filters);
    } else if (threadnum == 1) {
        result_time = execute_st_computation(dim, img_spec, args, filters);
    }

    if (result_time <= 0) {
        fprintf(stderr, "Error: Computation execution failed (returned time: %.4f).\n", result_time);
        goto cleanup;
    }

    st_write_logs(args, result_time);
    snprintf(output_filepath, sizeof(output_filepath), "output/result_%s", args->input_filename[0]); // Example output path
    sthreads_save(output_filepath, sizeof(output_filepath), threadnum, &img_result);
    status = 0;

cleanup:
    if (img_spec) {
        // Consider if img_spec needs a dedicated free function: free_img_spec(img_spec);
        free(img_spec);
    }
    bmp_img_free(&img_result);
    free(dim);
    bmp_img_free(&img);

    return status;
}

static double run_queue_mode(struct filter_mix *filters)
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
	double result_time = 0;
	int threadnum = 0;
	struct filter_mix *filters = NULL;

	args = malloc(sizeof(struct p_args));
	if (!args) 
		goto mem_err;

	threadnum = parse_args(argc, argv);

	filters = malloc(sizeof(struct filter_mix));
	if (!filters) {
		free(filters);
		goto mem_err;
	}
	init_filters(filters);

	if (threadnum < 0) {
		fprintf(stderr, "Error: couldn't parse the args\n");
		free(filters);
		free(args);
		return -2;
	}

	result_time = (!args->queue_mode) ? run_non_queue_mode(threadnum, filters) : run_queue_mode(filters);
	st_write_logs(args, result_time);

	free_filters(filters);
	return 0;

mem_err:
	fprintf(stderr, "Memory allocation error\n");
	free(args);
	return -1;
}
