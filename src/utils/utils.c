// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../libbmp/libbmp.h"
#include "../../logger/log.h"
#include "utils.h"
#include "args-parse.h"
#include <stdlib.h> 
#include <string.h> 
#include <stdio.h> 
#include <time.h> 
#include <sys/time.h>
#include <limits.h> 

const char *valid_tags[] = { "QPOP", "QPUSH", "READER", "WORKER", "WRITER", NULL };

/**
 * Swaps the values of two integers using pointers. Takes pointers to the integers
 * `a` and `b` as input. Returns void.
 */
void swap(int *a, int *b)
{
	int temp = *a;
	*a = *b;
	*b = temp;
}

/**
 * Finds the k-th smallest element in a subarray using the quickselect algorithm.
 * Operates on the integer array `data` within the range `[s, e)` (s inclusive, e exclusive).
 * `k` specifies the desired rank (0-based index) of the element to find within the
 * sorted version of the subarray. Modifies the input array `data` in place.
 */
int selectKth(int *data, int s, int e, int k)
{
	if (e - s <= 5) {
		for (int i = s + 1; i < e; i++)
			for (int j = i; j > s && data[j - 1] > data[j]; j--)
				swap(&data[j], &data[j - 1]);
		return data[s + k];
	}

	int p = (s + e) / 2;
	swap(&data[p], &data[e - 1]);

	int j = s;
	for (int i = s; i < e - 1; i++)
		if (data[i] < data[e - 1])
			swap(&data[i], &data[j++]);

	swap(&data[j], &data[e - 1]);

	if (k == j - s)
		return data[j];
	else if (k < j - s)
		return selectKth(data, s, j, k);
	else
		return selectKth(data, j + 1, e, k - (j - s + 1));
}


/**
 * Gets the current time as a double representing seconds since the epoch,
 * including microsecond precision. Returns the current time in seconds.
 */
double get_time_in_seconds(void)
{
	struct timeval time;
	gettimeofday(&time, NULL);
	return (double)time.tv_sec + (double)time.tv_usec * 1e-6;
}

/**
 * Converts a log tag enum value `tag` to its string representation based on
 * the `valid_tags` array. Returns a constant string representation of the tag,
 * or "unknown" if the tag is invalid.
 */
const char *log_tag_to_str(enum LOG_TAG tag)
{
	if (tag >= QPOP && tag <= WRITER) {
		return valid_tags[tag];
	}
	return "unknown";
}

/**
 * Appends a timing result to the queue-mode log file (QT_LOG_FILE_PATH).
 * Takes the measured time duration `result_time` (in seconds) and the
 * `tag` (enum LOG_TAG) identifying the operation being timed.
 * Errors opening the file are printed to stderr. Returns void.
 */
void qt_write_logs(double result_time, enum LOG_TAG tag)
{
	FILE *file = NULL;
	const char *log_tag_str = log_tag_to_str(tag);

	file = fopen(QT_LOG_FILE_PATH, "a");
	if (file) {
		fprintf(file, "%s %.6f\n", log_tag_str, result_time);
		fclose(file);
	} else {
		log_error("Error: could not open queue timing results file '%s' for appending.\n", QT_LOG_FILE_PATH);
	}
}

/**
 * Appends a timing result for single-threaded or non-queued multi-threaded execution
 * to the standard log file (ST_LOG_FILE_PATH), if logging is enabled via the `args`
 * structure. Also prints the result summary to stdout. Takes the `args` structure
 * containing execution parameters and the measured `result_time` (in seconds).
 * Errors opening the file are printed to stderr. Returns void.
 */
void st_write_logs(struct p_args *args, double result_time)
{
	FILE *file = NULL;

	if (!args || !args->log_enabled)
		return;

	file = fopen(ST_LOG_FILE_PATH, "a");
	const char *mode_str = (args->threadnum == 1 && args->compute_mode < 0) ? "none" : mode_to_str(args->compute_mode);
	const char *filter_str = args->filter_type ? args->filter_type : "unknown";

	if (file) {
		fprintf(file, "%s %d %s %d %.6f\n", filter_str, args->threadnum, mode_str, args->block_size, result_time);
		fclose(file);
	} else {
		log_error("Error: could not open standard timing results file '%s' for appending.\n", ST_LOG_FILE_PATH);
	}

	log_debug("RESULT: filter=%s, threadnum=%d, mode=%s, block=%d, time=%.6f seconds\n\n",
	       filter_str, args->threadnum, mode_str, args->block_size, result_time);
}

/**
 * Calculates an absolute time point in the future for use with timed waits.
 * Gets the current real time and adds a fixed nanosecond offset (NSEC_OFFSET).
 * Populates the `timespec` structure pointed to by `wait_time` with the result.
 * Returns void.
 */
void set_wait_time(struct timespec *wait_time) {
	clock_gettime(CLOCK_REALTIME, wait_time);
    wait_time->tv_nsec += NSEC_OFFSET;
    wait_time->tv_sec += wait_time->tv_nsec / 1000000000L;
    wait_time->tv_nsec %= 1000000000L;
}

/**
 * Compares two BMP images pixel by pixel to check if they are identical.
 * Takes pointers to the two `bmp_img` structures, `img1` and `img2`.
 * First checks if dimensions match. Returns 0 if images are identical,
 * -1 if dimensions differ or an error occurs (like NULL input or missing
 * pixel data), or 1 if pixel differences are found.
 */
int compare_images(const bmp_img *img1, const bmp_img *img2)
{
	int width, height;

	if (!img1 || !img2) {
		log_error("Error: Cannot compare NULL images.\n");
		return -1;
	}

	if (img1->img_header.biWidth != img2->img_header.biWidth || img1->img_header.biHeight != img2->img_header.biHeight) {
		log_debug("Image dimension mismatch: Img1(%ux%u) vs Img2(%ux%u)\n",
		       img1->img_header.biWidth, img1->img_header.biHeight,
		       img2->img_header.biWidth, img2->img_header.biHeight);
		return -1;
	}

	width = img1->img_header.biWidth;
	height = img1->img_header.biHeight;

	if (!img1->img_pixels || !img2->img_pixels) {
        log_error("Error: Cannot compare images with NULL pixel data.\n");
        return -1;
    }

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			bmp_pixel pixel1 = img1->img_pixels[y][x];
			bmp_pixel pixel2 = img2->img_pixels[y][x];

			if (pixel1.red != pixel2.red || pixel1.green != pixel2.green || pixel1.blue != pixel2.blue) {
				log_debug("Difference found at pixel (%d, %d):\n", x, y);
				log_debug("  Image 1 - R:%hhu G:%hhu B:%hhu\n", pixel1.red, pixel1.green, pixel1.blue);
				log_debug("  Image 2 - R:%hhu G:%hhu B:%hhu\n", pixel2.red, pixel2.green, pixel2.blue);
				return 1;
			}
		}
	}
	return 0; // Images are identical
}

