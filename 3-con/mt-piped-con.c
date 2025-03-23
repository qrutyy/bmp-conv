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

#define THREAD_NUM 1
#define LOG_FILE_PATH "tests/timing-results.dat"
#define MAX_QUEUE_SIZE 10

char **input_files;
int file_count = 0;
int block_size = 0;
const char *output_filename = NULL;
const char *filter_type;
const char *mode_str;
enum compute_mode mode;
char output_filepath[MAX_PATH_LEN];

int next_x_block = 0;
int next_y_block = 0;
pthread_mutex_t x_block_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t y_block_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t xy_block_mutex = PTHREAD_MUTEX_INITIALIZER;

// #shitty
struct filter blur, motion_blur, gaus_blur, conv, sharpen, embos, big_gaus;

struct img_queue {
	bmp_img images[MAX_QUEUE_SIZE];
	int front, rear, size;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} input_queue, output_queue;

void queue_init(struct img_queue *q)
{
	q->front = q->rear = q->size = 0;
	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->cond, NULL);
}

void queue_push(struct img_queue *q, bmp_img img)
{
	pthread_mutex_lock(&q->mutex);
	while (q->size == MAX_QUEUE_SIZE)
		pthread_cond_wait(&q->cond, &q->mutex);
	q->images[q->rear] = img;
	q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
	q->size++;
	pthread_cond_signal(&q->cond);
	pthread_mutex_unlock(&q->mutex);
}

bmp_img queue_pop(struct img_queue *q)
{
	pthread_mutex_lock(&q->mutex);
	while (q->size == 0)
		pthread_cond_wait(&q->cond, &q->mutex);
	bmp_img img = q->images[q->front];
	q->front = (q->front + 1) % MAX_QUEUE_SIZE;
	q->size--;
	pthread_cond_signal(&q->cond);
	pthread_mutex_unlock(&q->mutex);
	return img;
}

int parse_args(int argc, char *argv[], char ***input_files, int *file_count)
{
	int threadnum = 0;

	if (argc < 5) {
		printf("Usage: %s <input_images...> <filter_type> --threadnum=N --mode=MODE --block=N [--output=NAME]\n",
		       argv[0]);
		return -1;
	}

	int param_start = 2;
	while (param_start < argc && strncmp(argv[param_start], "--", 2) != 0) {
		param_start++;
	}

	if (param_start < 3) {
		fprintf(stderr, "Error: At least one input image required\n");
		return -1;
	}

	*file_count = param_start - 2;
	*input_files = malloc(*file_count * sizeof(char *));
	if (!*input_files) {
		fprintf(stderr, "Memory allocation error\n");
		return -1;
	}

	for (int i = 0; i < *file_count; i++) {
		(*input_files)[i] = argv[i + 1];
	}

	filter_type = argv[param_start - 1];
	printf("Filter type: %s\n", filter_type);

	for (int i = param_start; i < argc; i++) {
		if (strncmp(argv[i], "--threadnum=", 12) == 0) {
			threadnum = atoi(argv[i] + 12);
			printf("Number of threads: %d\n", threadnum);
		} else if (strncmp(argv[i], "--mode=", 7) == 0) {
			mode_str = argv[i] + 7;
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
			printf("Mode selected: %s\n", mode_str);
		} else if (strncmp(argv[i], "--block=", 8) == 0) {
			block_size = atoi(argv[i] + 8);
			if (block_size <= 0) {
				fprintf(stderr, "Error: Block size must be at least 1\n");
				return -1;
			}
		} else if (strncmp(argv[i], "--output=", 9) == 0) {
			output_filename = argv[i] + 9;
			printf("Output filename base: %s\n", output_filename);
		} else {
			fprintf(stderr, "Error: Unrecognized argument %s\n", argv[i]);
			return -1;
		}
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
	} else if (strcmp(filter_type, "gg") == 0) {
		apply_filter(spec, big_gaus);
	} else {
		fprintf(stderr, "Wrong filter type parameter\n");
	}
}

void *reader_thread(void *arg)
{
	for (int i = 0; i < file_count; i++) {
		bmp_img img;
		char filepath[MAX_PATH_LEN];
		snprintf(filepath, sizeof(filepath), "../test-img/%s", input_files[i]);
		if (bmp_img_read(&img, filepath)) {
			fprintf(stderr, "Error: Could not open %s\n", input_files[i]);
			continue;
		}
		queue_push(&input_queue, img);
		printf("Input img first pix: %d %d %d\n", img.img_pixels[0][0].red, img.img_pixels[0][0].green,
		       img.img_pixels[0][0].blue);
		printf("Successfully read %s\n", filepath);
	}
	for (int i = 0; i < THREAD_NUM; i++) {
		bmp_img empty_img = { 0 }; // Special empty image as termination signal
		queue_push(&input_queue, empty_img);
	}
	return NULL;
}

void *worker_thread(void *arg)
{
	while (1) {
		bmp_img img = queue_pop(&input_queue);
		if (img.img_header.biWidth == 0 && img.img_header.biHeight == 0) {
			break;
		}

		bmp_img *img_result = malloc(sizeof(bmp_img));
		if (!img_result) {
			fprintf(stderr, "Memory allocation failed\n");
			exit(EXIT_FAILURE);
		}
		struct thread_spec *th_spec = malloc(sizeof(struct thread_spec)); // TODO add mem check
		bmp_img_init_df(img_result, img.img_header.biWidth, img.img_header.biHeight);

		struct img_spec *img_spec = init_img_spec(&img, img_result);
		struct img_dim *dim = init_dimensions(img.img_header.biWidth, img.img_header.biHeight);
		th_spec->dim = dim;
		th_spec->img = img_spec;

		while (1) {
			switch (mode) {
			case BY_ROW:
				if (process_by_row(th_spec, &next_x_block, block_size, &x_block_mutex))
					goto exit;
				break;
			case BY_COLUMN:
				if (process_by_column(th_spec, &next_y_block, block_size, &y_block_mutex))
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
				fprintf(stderr, "Invalid mode\n");
				free(th_spec);
				return NULL;
			}

			filter_part_computation(th_spec);
			printf("Input img_result afterr first pix: %d %d %d\n",
			       th_spec->img->output_img->img_pixels[0][0].red, img_result->img_pixels[0][0].green,
			       img_result->img_pixels[0][0].blue);
		}

	exit:
		queue_push(&output_queue, *img_result);
		bmp_img_free(&img);
		next_y_block = 0;
		next_x_block = 0;
	}
	printf("Successfully processed\n");
	return NULL;
}

void *writer_thread(void *arg)
{
	printf("File count: %d\n", file_count);
	for (int i = 0; i < file_count; i++) {
		bmp_img img = queue_pop(&output_queue);
		printf("ppopped\n");
		if (img.img_header.biWidth == 0 && img.img_header.biHeight == 0) {
			break; // Stop when termination signal is received
		}
		if (output_filename)
			snprintf(output_filepath, sizeof(output_filepath), "../test-img/%s_%d.bmp", output_filename, i);
		else
			snprintf(output_filepath, sizeof(output_filepath), "../test-img/output_%d.bmp", i);
		bmp_img_write(&img, output_filepath);
		bmp_img_free(&img);
		printf("Successfully write%s\n", output_filepath);
	}
	return NULL;
}

int main(int argc, char *argv[])
{
	pthread_t reader, writer, workers[THREAD_NUM];
	queue_init(&input_queue);
	queue_init(&output_queue);
	init_filters(&blur, &motion_blur, &gaus_blur, &conv, &sharpen, &embos, &big_gaus);

	if (parse_args(argc, argv, &input_files, &file_count) < 0) {
		fprintf(stderr, "Error parsing arguments\n");
		return -1;
	}

	pthread_create(&reader, NULL, reader_thread, NULL);
	for (int i = 0; i < THREAD_NUM; i++)
		pthread_create(&workers[i], NULL, worker_thread, NULL);
	pthread_create(&writer, NULL, writer_thread, NULL);

	pthread_join(reader, NULL);
	for (int i = 0; i < THREAD_NUM; i++)
		pthread_join(workers[i], NULL);
	pthread_join(writer, NULL);

	free(input_files);
	return 0;
}
