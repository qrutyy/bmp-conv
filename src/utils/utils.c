// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h>

const double motion_blur_arr[9][9] = { { 1, 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 1, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0, 1, 0, 0, 0, 0, 0, 0 },
				       { 0, 0, 0, 1, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 1, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 1, 0, 0, 0 },
				       { 0, 0, 0, 0, 0, 0, 1, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 1, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 1 } };

const double blur_arr[5][5] = { { 0, 0, 1, 0, 0 }, { 0, 1, 1, 1, 0 }, { 1, 1, 1, 1, 1 }, { 0, 1, 1, 1, 0 }, { 0, 0, 1, 0, 0 } };

const double gaus_blur_arr[5][5] = { { 1, 4, 6, 4, 1 }, { 4, 16, 24, 16, 4 }, { 6, 24, 36, 24, 6 }, { 4, 16, 24, 16, 4 }, { 1, 4, 6, 4, 1 } };

const double conv_arr[3][3] = { { 0, 0, 0 }, { 0, 1, 0 }, { 0, 0, 0 } };

const double sharpen_arr[3][3] = { { -1, -1, -1 }, { -1, 9, -1 }, { -1, -1, -1 } };

const double emboss_arr[5][5] = { { -1, -1, -1, -1, 0 }, { -1, -1, -1, 0, 1 }, { -1, -1, 0, 1, 1 }, { -1, 0, 1, 1, 1 }, { 0, 1, 1, 1, 1 } };

const double big_gaus_arr[15][15] = {
	{ 2, 2, 3, 3, 4, 4, 5, 5, 5, 4, 4, 3, 3, 2, 2 },      { 2, 3, 3, 4, 4, 5, 5, 6, 5, 5, 4, 4, 3, 3, 2 },	      { 3, 3, 4, 5, 5, 6, 6, 7, 6, 6, 5, 5, 4, 3, 3 },
	{ 3, 4, 5, 6, 7, 7, 8, 8, 8, 7, 7, 6, 5, 4, 3 },      { 4, 4, 5, 7, 8, 9, 9, 10, 9, 9, 8, 7, 5, 4, 4 },	      { 4, 5, 6, 7, 9, 10, 11, 11, 11, 10, 9, 7, 6, 5, 4 },
	{ 5, 5, 6, 8, 9, 11, 12, 12, 12, 11, 9, 8, 6, 5, 5 }, { 5, 6, 7, 8, 10, 11, 12, 13, 12, 11, 10, 8, 7, 6, 5 }, { 5, 5, 6, 8, 9, 11, 12, 12, 12, 11, 9, 8, 6, 5, 5 },
	{ 4, 5, 6, 7, 9, 10, 11, 11, 11, 10, 9, 7, 6, 5, 4 }, { 4, 4, 5, 7, 8, 9, 9, 10, 9, 9, 8, 7, 5, 4, 4 },	      { 3, 4, 5, 6, 7, 7, 8, 8, 8, 7, 7, 6, 5, 4, 3 },
	{ 3, 3, 4, 5, 5, 6, 6, 7, 6, 6, 5, 5, 4, 3, 3 },      { 2, 3, 3, 4, 4, 5, 5, 6, 5, 5, 4, 4, 3, 3, 2 },	      { 2, 2, 3, 3, 4, 4, 5, 5, 5, 4, 4, 3, 3, 2, 2 }
};

const double med_gaus_arr[9][9] = { { 1, 1, 2, 2, 2, 2, 2, 1, 1 }, { 1, 2, 2, 3, 3, 3, 2, 2, 1 }, { 2, 2, 3, 4, 5, 4, 3, 2, 2 },
				    { 2, 3, 4, 5, 6, 5, 4, 3, 2 }, { 2, 3, 5, 6, 7, 6, 5, 3, 2 }, { 2, 3, 4, 5, 6, 5, 4, 3, 2 },
				    { 2, 2, 3, 4, 5, 4, 3, 2, 2 }, { 1, 2, 2, 3, 3, 3, 2, 2, 1 }, { 1, 1, 2, 2, 2, 2, 2, 1, 1 } };

const double box_blur_arr[15][15] = {
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }
};

const char *valid_filters[] = { "bb", "mb", "em", "gg", "gb", "co", "sh", "mm", "bo", "mg", NULL };
const char *valid_modes[] = { "by_row", "by_column", "by_pixel", "by_grid", NULL };
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

char *check_filter_arg(char *filter)
{
	for (int i = 0; valid_filters[i] != NULL; i++) {
		if (strcmp(filter, valid_filters[i]) == 0) {
			return filter;
		}
	}
	fputs("Error: Wrong filter.\n", stderr);
	return NULL;
}

int check_mode_arg(char *mode_str)
{
	for (int i = 0; valid_modes[i] != NULL; i++) {
		if (strcmp(mode_str, valid_modes[i]) == 0) {
			return i;
		}
	}
	fputs("Error: Invalid mode.\n", stderr);
	return -1;
}

const char *mode_to_str(int mode)
{
	if (mode >= 0 && (unsigned long)mode < (sizeof(valid_modes) / sizeof(valid_modes[0]) - 1)) {
		return valid_modes[mode];
	}
	if (mode == -1) {
		return "unset/invalid";
	}
	return "unknown";
}

static const char *log_tag_to_str(enum LOG_TAG tag)
{
	if (tag >= 0 && (unsigned long)tag < (sizeof(valid_tags) / sizeof(valid_tags[0]) - 1)) {
		return valid_tags[tag];
	}
	return "unknown";
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

void set_wait_time(struct timespec *wait_time) {
	clock_gettime(CLOCK_REALTIME, wait_time);
    wait_time->tv_nsec += NSEC_OFFSET;
    wait_time->tv_sec += wait_time->tv_nsec / 1000000000;
    wait_time->tv_nsec %= 1000000000;
	return;
}

void initialize_args(struct p_args *args_ptr)
{
	args_ptr->threadnum = 1;
	args_ptr->block_size = 0;
	args_ptr->output_filename = "";
	args_ptr->filter_type = NULL;
	args_ptr->compute_mode = -1;
	args_ptr->log_enabled = 0;
	args_ptr->queue_mode = 0;
	args_ptr->wrt_count = 0;
	args_ptr->ret_count = 0;
	args_ptr->wot_count = 0;
	args_ptr->file_count = 0;
	args_ptr->queue_memory_limit = 500 * 1024 * 1024;
	for (int i = 0; i < MAX_IMAGE_QUEUE_SIZE; ++i) {
		args_ptr->input_filename[i] = NULL;
	}
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

static void init_filter(struct filter **f, int size, double bias, double factor, const double arr[size][size])
{
	*f = malloc(sizeof(struct filter));
	if (!*f) {
		fprintf(stderr, "Memory allocation failed\n");
		exit(1);
	}
	(*f)->size = size;
	(*f)->bias = bias;
	(*f)->factor = factor;

	(*f)->filter_arr = malloc(size * sizeof(double *));
	for (int i = 0; i < size; i++) {
		(*f)->filter_arr[i] = malloc(size * sizeof(double));
		memcpy((*f)->filter_arr[i], arr[i], size * sizeof(double));
	}
}

static void free_filter(struct filter *f)
{
	for (int i = 0; i < f->size; i++) {
		free(f->filter_arr[i]);
	}
	free(f->filter_arr);
	free(f);
}

void init_filters(struct filter_mix *filters)
{
	init_filter(&filters->motion_blur, 9, 0.0, 1.0 / 9.0, motion_blur_arr);
	init_filter(&filters->blur, 5, 0.0, 1.0 / 13.0, blur_arr);
	init_filter(&filters->gaus_blur, 5, 0.0, 1.0 / 256.0, gaus_blur_arr);
	init_filter(&filters->conv, 3, 0.0, 1.0, conv_arr);
	init_filter(&filters->sharpen, 3, 0.0, 1.0, sharpen_arr);
	init_filter(&filters->emboss, 5, 128.0, 1.0, emboss_arr);
	init_filter(&filters->big_gaus, 15, 0.0, 1.0 / 771, big_gaus_arr);
	init_filter(&filters->med_gaus, 9, 0.0, 1.0 / 213, med_gaus_arr);
	init_filter(&filters->box_blur, 15, 0.0, 1.0 / 225, box_blur_arr);
}

void free_filters(struct filter_mix *filters)
{
	free_filter(filters->motion_blur);
	free_filter(filters->blur);
	free_filter(filters->gaus_blur);
	free_filter(filters->conv);
	free_filter(filters->sharpen);
	free_filter(filters->emboss);
	free_filter(filters->big_gaus);
	free_filter(filters->med_gaus);
	free_filter(filters->box_blur);
}


int parse_mandatory_args(int argc, char *argv[], struct p_args *args) {
	for (int i = 2; i < argc; i++) {
		if (strncmp(argv[i], "--filter=", 9) == 0) {
			args->filter_type = check_filter_arg(argv[i] + 9);
			if (!args->filter_type) return -1;
			argv[i] = "_";
		} else if (strncmp(argv[i], "--mode=", 7) == 0) {
			args->compute_mode = check_mode_arg(argv[i] + 7);
			if (args->compute_mode < 0) return -1;
			argv[i] = "_";
		} else if (strncmp(argv[i], "--block=", 8) == 0) {
			args->block_size = atoi(argv[i] + 8);
			if (args->block_size <= 0) {
				fprintf(stderr, "Error: Block size must be >= 1.\n");
				return -1;
			}
			argv[i] = "_";
		}
	}
	return 0;
}

int parse_queue_mode_args(int argc, char *argv[], struct p_args *args) {
	uint8_t rww_found = 0;
	int wrt_temp = 0, ret_temp = 0, wot_temp = 0;
	char *rww_values = NULL;

	for (int i = 2; i < argc; i++) {
		if (strncmp(argv[i], "--log=", 6) == 0) {
			args->log_enabled = atoi(argv[i] + 6);
		} else if (strncmp(argv[i], "--lim=", 6) == 0) {
			args->queue_memory_limit = atoi(argv[i] + 6) * 1024 * 1024;
		} else if (strncmp(argv[i], "--output=", 9) == 0) {
			args->output_filename = argv[i] + 9;
		} else if (strncmp(argv[i], "--rww=", 6) == 0) {
			rww_values = argv[i] + 6;
			if (sscanf(rww_values, "%d,%d,%d", &ret_temp, &wot_temp, &wrt_temp) != 3) {
				fprintf(stderr, "Error: Invalid format for --rww. Expected --rww=W,R,T.\n");
				return -1;
			}
			if (wrt_temp <= 0 || wrt_temp > UCHAR_MAX || ret_temp <= 0 || ret_temp > UCHAR_MAX || wot_temp <= 0 || wot_temp > UCHAR_MAX) {
				fprintf(stderr, "Error: Thread counts in --rww must be between 1 and %d.\n", UCHAR_MAX);
				return -1;
			}
			args->wrt_count = wrt_temp;
			args->ret_count = ret_temp;
			args->wot_count = wot_temp;
			rww_found = 1;
			argv[i] = "_";
		} else if (strncmp(argv[i], "_", 1) == 0) {
			continue;
		} else if (args->file_count < MAX_IMAGE_QUEUE_SIZE) {
			args->input_filename[args->file_count++] = argv[i];
		} else {
			fprintf(stderr, "Error: Unknown argument %s\n", argv[i]);
			return -1;
		}
	}

	if (!rww_found) {
		fprintf(stderr, "Error: queue-based mode requires --rww=W,R,T argument.\n");
		return -1;
	}
	if ((args->ret_count + args->wot_count + args->wrt_count) < 3) {
		fprintf(stderr, "Error: queue-based mode requires at least 3 threads in total.\n");
		return -1;
	}

	return 0;
}

int parse_normal_mode_args(int argc, char *argv[], struct p_args *args) {
	for (int i = 1; i < argc; i++) {
		if (strncmp(argv[i], "--threadnum=", 12) == 0) {
			args->threadnum = atoi(argv[i] + 12);
			if (args->threadnum <= 0) {
				fprintf(stderr, "Error: Invalid threadnum.\n");
				return -1;
			}
		} else if (strncmp(argv[i], "--log=", 6) == 0) {
			args->log_enabled = atoi(argv[i] + 6);
		} else if (strncmp(argv[i], "--output=", 9) == 0) {
			args->output_filename = argv[i] + 9;
		} else if (args->input_filename[0] == NULL && strncmp(argv[i], "_", 1)) {
			args->input_filename[0] = argv[i];
			args->file_count++;
		} else if (strncmp(argv[i], "_", 1) == 0) {
			continue;
		} else {
			fprintf(stderr, "Error: Unknown argument %s\n", argv[i]);
			return -1;
		}
	}

	if (args->file_count != 1) {
		fprintf(stderr, "Error: non-queued mode requires exactly 1 input image.\n");
		return -1;
	}

	return 0;
}
