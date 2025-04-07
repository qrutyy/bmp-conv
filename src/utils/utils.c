// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h>
#include "args-parse.h"

const char *valid_tags[] = { "QPOP", "QPUSH", "READER", "WORKER", "WRITER", NULL };

void swap(int *a, int *b)
{
	int temp = *a;
	*a = *b;
	*b = temp;
}

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

double get_time_in_seconds(void)
{
	struct timeval time;
	gettimeofday(&time, NULL);
	return (double)time.tv_sec + (double)time.tv_usec * 0.000001;
}

const char *log_tag_to_str(enum LOG_TAG tag)
{
	if (tag >= 0 && (unsigned long)tag < (sizeof(valid_tags) / sizeof(valid_tags[0]) - 1)) {
		return valid_tags[tag];
	}
	return "unknown";
}

void qt_write_logs(double result_time, enum LOG_TAG tag)
{
	FILE *file = NULL;

	file = fopen(QT_LOG_FILE_PATH, "a");
	const char *log_tag_str = log_tag_to_str(tag);

	if (file) {
		fprintf(file, "%s %.6f\n", log_tag_str, result_time);
		fclose(file);
	} else {
		fputs("Error: could not open timing results file for appending.\n", stderr);
	}

	return;
}
void st_write_logs(struct p_args *args, double result_time)
{
	FILE *file = NULL;

	if (!args || !args->log_enabled)
		return;

	file = fopen(ST_LOG_FILE_PATH, "a");
	const char *mode_str = (args->threadnum == 1) ? "none" : mode_to_str(args->compute_mode);

	if (file) {
		fprintf(file, "%s %d %s %d %.6f\n", args->filter_type ? args->filter_type : "unknown", args->threadnum, mode_str, args->block_size, result_time);
		fclose(file);
	} else {
		fputs("Error: could not open timing results file for appending.\n", stderr);
	}

	printf("RESULT: filter = %s, threadnum = %d, mode = %s, block = %d, time = %.6f seconds\n\n", args->filter_type ? args->filter_type : "unknown", args->threadnum, mode_str,
	       args->block_size, result_time);
	return;
}



void set_wait_time(struct timespec *wait_time) {
	clock_gettime(CLOCK_REALTIME, wait_time);
    wait_time->tv_nsec += NSEC_OFFSET;
    wait_time->tv_sec += wait_time->tv_nsec / 1000000000;
    wait_time->tv_nsec %= 1000000000;
	return;
}

int compare_images(const bmp_img *img1, const bmp_img *img2)
{
	int width, height = 0;
	if (img1->img_header.biWidth != img2->img_header.biWidth || img1->img_header.biHeight != img2->img_header.biHeight) {
		printf("Images have different dimensions!\n");
		return -1;
	}

	width = img1->img_header.biWidth;
	height = img1->img_header.biHeight;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			bmp_pixel pixel1 = img1->img_pixels[y][x];
			bmp_pixel pixel2 = img2->img_pixels[y][x];

			if (pixel1.red != pixel2.red || pixel1.green != pixel2.green || pixel1.blue != pixel2.blue) {
				printf("Difference found at pixel (%d, %d):\n", x, y);
				printf("Image 1 - R:%d G:%d B:%d\n", pixel1.red, pixel1.green, pixel1.blue);
				printf("Image 2 - R:%d G:%d B:%d\n", pixel2.red, pixel2.green, pixel2.blue);
				return 1;
			}
		}
	}
	return 0;
}

