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
enum compute_mode { BY_ROW, BY_COLUMN, BY_PIXEL, BY_GRID };
extern const char *valid_modes[];

/**
 * Swaps the values of two integers using pointers. Takes pointers to the integers
 * `a` and `b` as input.
 *
 * @return void.
 */
void swap(int *a, int *b);

/**
 * Finds the k-th smallest element in a subarray using the quickselect algorithm.
 * Operates on the integer array `data` within the range `[s, e)` (s inclusive, e exclusive).
 * `k` specifies the desired rank (0-based index) of the element to find within the
 * sorted version of the subarray. Modifies the input array `data` in place.
 */
int selectKth(int *data, int s, int e, int k);

/**
 * Gets the current time as a double representing seconds since the epoch,
 * including microsecond precision.
 *
 * @return current time in seconds.
 */
double get_time_in_seconds(void);
const char *compute_mode_to_str(enum compute_mode);

/**
 * Converts a log tag enum value `tag` to its string representation based on
 * the `valid_tags` array.
 *
 * @return constant string representation of the tag, or "unknown" if the tag is invalid.
 */
const char *log_tag_to_str(enum LOG_TAG tag);

/**
 * Appends a timing result to the queue-mode log file (QT_LOG_FILE_PATH).
 * Takes the measured time duration `result_time` (in seconds) and the
 * `tag` (enum LOG_TAG) identifying the operation being timed.
 * Errors opening the file are printed to stderr.
 *
 * @return void.
 */
void qt_write_logs(double result_time, enum LOG_TAG tag, const char *compute_mode_str);

/**
 * Appends a timing result for single-threaded or non-queued multi-threaded execution
 * to the standard log file (ST_LOG_FILE_PATH), if logging is enabled via the `args`
 * structure. Also prints the result summary to stdout. Takes the `args` structure
 * containing execution parameters and the measured `result_time` (in seconds).
 * Errors opening the file are printed to stderr.
 *
 * @return void.
 */
void st_write_logs(struct p_args *args, double result_time);

/**
 * Calculates an absolute time point in the future for use with timed waits.
 * Gets the current real time and adds a fixed nanosecond offset (NSEC_OFFSET).
 * Populates the `timespec` structure pointed to by `wait_time` with the result.
 *
 * @return void.
 */
void set_wait_time(struct timespec *wait_time);

/**
 * Compares two BMP images pixel by pixel to check if they are identical.
 * Takes pointers to the two `bmp_img` structures, `img1` and `img2`.
 * First checks if dimensions match.
 *
 * @return 0 if images are identical,
 *		  -1 if dimensions differ or an error occurs (like NULL input or missing
 * pixel data)
 *		   1 if pixel differences are found.
 */
int compare_images(const bmp_img *img1, const bmp_img *img2);
