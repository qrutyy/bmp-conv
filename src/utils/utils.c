// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils.h"
#include "libbmp/libbmp.h"
#include "logger/log.h"
#include "args-parse.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

const char *valid_tags[] = { "QPOP", "QPUSH", "READER", "WORKER", "WRITER", NULL };
const char *valid_modes[] = { "by_row", "by_column", "by_pixel", "by_grid" };

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
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
		log_error("clock_gettime(CLOCK_MONOTONIC) failed");
		return 0.0;
	}
	return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

const char *compute_mode_to_str(enum conv_compute_mode mode)
{
	if ((size_t)mode <= 3) {
		return valid_modes[mode];
	}
	return "unknown";
}

const char *log_tag_to_str(enum LOG_TAG tag)
{
	if (tag >= QPOP && tag <= WRITER) {
		return valid_tags[tag];
	}
	return "unknown";
}

void ensure_log_dir_exists(const char *file_path)
{
	char *dir_path;
	char *slash;
	struct stat st = {0};

	if (!file_path) return;

	dir_path = strdup(file_path);
	if (!dir_path) return;

	slash = strrchr(dir_path, '/');
	if (slash) {
		*slash = '\0';
		if (stat(dir_path, &st) == -1) {
			if (mkdir(dir_path, 0700) == -1 && errno != EEXIST) {
				log_error("Error: Could not create directory '%s': %s\n", dir_path, strerror(errno));
			}
		}
	}
	free(dir_path);
}

void qt_write_logs(double result_time, enum LOG_TAG tag, const char *compute_mode_str)
{
	FILE *file = NULL;
	const char *log_tag_str = log_tag_to_str(tag);

	const char *mode_str = (compute_mode_str && strlen(compute_mode_str) > 0) ? compute_mode_str : "unknown";

	ensure_log_dir_exists(CPU_QT_LOG_FILE_PATH);
	file = fopen(CPU_QT_LOG_FILE_PATH, "a");
	if (file) {
		// MODE TAG TIME
		fprintf(file, "%s %s %.6f\n", mode_str, log_tag_str, result_time);
		fclose(file);
	} else {
		log_error("Error: could not open queue timing results file '%s' for appending.\n", CPU_QT_LOG_FILE_PATH);
	}
}

char *get_log_file_path(enum conv_backend backend)
{
	if (backend == CONV_BACKEND_GPU)
		return (char *)GPU_LOG_FILE_PATH;
	return (char *)CPU_LOG_FILE_PATH;
}

static const char *backend_to_str(enum conv_backend backend)
{
	return backend == CONV_BACKEND_GPU ? "GPU" : "CPU";
}

static const char *exec_mode_to_str(enum conv_queue_mode queue, enum conv_mpi_mode mpi)
{
	if (mpi == CONV_MPI_ENABLED)
		return "mpi";
	if (queue == CONV_QUEUE_ENABLED)
		return "queue";
	return "normal";
}

void write_logs(struct p_args *args, double result_time, enum conv_backend backend)
{
	const char *file_path;
	FILE *file = NULL;

	if (!args || !args->log_enabled)
		return;

	file_path = get_log_file_path(backend);

	ensure_log_dir_exists(file_path);
	file = fopen(file_path, "a");
	const char *compute_mode_str = (args->compute_ctx.threadnum == 1 && args->compute_cfg.compute_mode < 0) ? "none" : compute_mode_to_str(args->compute_cfg.compute_mode);
	const char *filter_str = args->compute_cfg.filter_type ? args->compute_cfg.filter_type : "unknown";
	const char *backend_str = backend_to_str(backend);
	const char *exec_mode_str = exec_mode_to_str(args->compute_cfg.queue, args->compute_cfg.mpi);

	if (file) {
		/* Unified format: Backend Mode Filter ThreadNum ComputeMode BlockSize Result (RunID ProcessNum are prepended by benchmark scripts) */
		fprintf(file, "%s %s %s %d %s %d %.6f\n",
			backend_str, exec_mode_str, filter_str,
			args->compute_ctx.threadnum, compute_mode_str,
			args->compute_cfg.block_size, result_time);
		fclose(file);
	} else {
		log_error("Error: could not open timing results file '%s' for appending.\n", file_path);
	}

	log_debug("RESULT: backend=%s mode=%s filter=%s threadnum=%d compute_mode=%s block=%d time=%.6f seconds\n\n",
		backend_str, exec_mode_str, filter_str, args->compute_ctx.threadnum, compute_mode_str, args->compute_cfg.block_size, result_time);
}

void set_wait_time(struct timespec *wait_time)
{
	clock_gettime(CLOCK_REALTIME, wait_time);
	wait_time->tv_nsec += NSEC_OFFSET;
	wait_time->tv_sec += wait_time->tv_nsec / 1000000000L;
	wait_time->tv_nsec %= 1000000000L;
}

int compare_images(const bmp_img *img1, const bmp_img *img2)
{
	int width, height;

	if (!img1 || !img2) {
		log_error("Error: Cannot compare NULL images.\n");
		return -1;
	}

	if (img1->img_header.biWidth != img2->img_header.biWidth || img1->img_header.biHeight != img2->img_header.biHeight) {
		log_debug("Image dimension mismatch: Img1(%ux%u) vs Img2(%ux%u)\n", img1->img_header.biWidth, img1->img_header.biHeight, img2->img_header.biWidth,
			  img2->img_header.biHeight);
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
