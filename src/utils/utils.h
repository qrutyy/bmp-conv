// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../../libbmp/libbmp.h"
#include <stdio.h>
#include <stdint.h>

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define MAX_PATH_LEN 40
#define MAX_FILTER_SIZE 9
#define PADDING (cfilter.size / 2)
#define MAX_FILTERS 10
#define MAX_IMAGE_QUEUE_SIZE 20
#define ST_LOG_FILE_PATH "tests/timing-results.dat"
#define QT_LOG_FILE_PATH "tests/queue-timings.dat"

enum LOG_TAG { QPOP, QPUSH, READER, WORKER, WRITER };

struct filter {
	int size;
	double bias;
	double factor;
	double **filter_arr;
};

struct filter_mix {
	struct filter *blur;
	struct filter *motion_blur;
	struct filter *gaus_blur;
	struct filter *conv;
	struct filter *sharpen;
	struct filter *emboss;
	struct filter *big_gaus;
	struct filter *med_gaus;
	struct filter *box_blur;
};

struct p_args {
	uint8_t block_size;
	char *input_filename[MAX_IMAGE_QUEUE_SIZE];
	char *output_filename;
	uint8_t file_count;
	char *filter_type;
	int8_t compute_mode;
	uint8_t log_enabled;
	uint8_t queue_mode;
	int8_t threadnum;
	uint8_t wrt_count; // writer threads count
	uint8_t ret_count; // reader threads count
	uint8_t wot_count; // worker threads count
	size_t queue_memory_limit;
};

void swap(int *a, int *b);
int selectKth(int *data, int s, int e, int k);
void initialize_args(struct p_args *args_ptr);
double get_time_in_seconds(void);
const char *mode_to_str(int mode);
void st_write_logs(struct p_args *args, double result_time);
void qt_write_logs(double result_time, enum LOG_TAG tag);
int check_mode_arg(char *mode_str);
char *check_filter_arg(char *filter);
int compare_images(const bmp_img *img1, const bmp_img *img2);
void free_filters(struct filter_mix *filters);
void init_filters(struct filter_mix *filters);
