// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#define _POSIX_C_SOURCE 200809L // to use functions from posix 

#include "../../libbmp/libbmp.h"
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include "args-parse.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define MAX_PATH_LEN 40
#define MAX_FILTER_SIZE 9
#define PADDING (cfilter.size / 2)
#define MAX_FILTERS 10
#define ST_LOG_FILE_PATH "tests/timing-results.dat"
#define NSEC_OFFSET (1000 * 1000000) // 1000 ms in nanoseconds
#define QT_LOG_FILE_PATH "tests/queue-timings.dat"

enum LOG_TAG { QPOP, QPUSH, READER, WORKER, WRITER };

void swap(int *a, int *b);
int selectKth(int *data, int s, int e, int k);

double get_time_in_seconds(void);
const char *mode_to_str(int mode);
const char *log_tag_to_str(enum LOG_TAG tag);

void qt_write_logs(double result_time, enum LOG_TAG tag);
void st_write_logs(struct p_args *args, double result_time);

void set_wait_time(struct timespec *wait_time);
int compare_images(const bmp_img *img1, const bmp_img *img2);

void free_filters(struct filter_mix *filters);
void init_filters(struct filter_mix *filters);

