// SPDX-License-Identifier: GPL-3.0-or-later

#include "threads.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "../../libbmp/libbmp.h"
#include "../../logger/log.h"
#include "../utils/threads-general.h"
#include "../mt-mode/compute.h"
#include "../utils/utils.h"
#include "queue.h"

// Global counters for tracking file progress across threads
size_t written_files = 0;
size_t read_files = 0;

void *reader_thread(void *arg)
{
	struct qthreads_gen_info *qt_info = (struct qthreads_gen_info *)arg;
	size_t i = 0;
	bmp_img *img;
	char filepath[MAX_PATH_LEN];
	bmp_img *empty_img = NULL;
	double start_time = 0;
	double result_time = 0;
	size_t read_files_local = 0;
	const char *mode_str = NULL;
	log_debug("Reader thread started.");

	mode_str = compute_mode_to_str(qt_info->pargs->compute_cfg.compute_mode);

	while (1) {
		read_files_local = __atomic_fetch_add(&read_files, 1, __ATOMIC_ACQUIRE);
		if (read_files_local >= qt_info->pargs->files_cfg.file_cnt) {
			__atomic_fetch_sub(&read_files, 1, __ATOMIC_RELEASE);
			break;
		}

		start_time = get_time_in_seconds();

		img = malloc(sizeof(bmp_img));
		if (!img) {
			log_error("Reader Error: Failed to allocate bmp_img struct");
			break;
		}

		snprintf(filepath, sizeof(filepath), "test-img/%s", qt_info->pargs->files_cfg.input_filename[read_files_local]);

		if (bmp_img_read(img, filepath) != 0) {
			log_error("Reader Error: Could not read BMP file '%s'", filepath);
			free(img);
			exit(EXIT_FAILURE);
		}

		queue_push(qt_info->input_q, img, qt_info->pargs->files_cfg.input_filename[read_files_local], mode_str);

		result_time = get_time_in_seconds() - start_time;
		if (result_time > 0)
			qt_write_logs(result_time, READER, mode_str);

		log_debug("Reader: Pushed '%s' to input queue.", filepath);
	}

	log_debug("Reader: Finished reading files. Waiting at barrier.");
	pthread_barrier_wait(qt_info->reader_barrier);

	log_debug("Reader: Barrier passed. Sending termination signals.");
	for (i = 0; i < (size_t)(qt_info->pargs->mt_mode_cfg.qm.threads_cfg.worker_cnt); i++) {
		empty_img = calloc(1, sizeof(bmp_img));
		if (empty_img) {
			queue_push(qt_info->input_q, empty_img, NULL, mode_str);
		} else {
			log_error("Reader Error: Failed to allocate termination signal");
		}
	}

	log_debug("Reader: thread finished.");
	return NULL;
}

/**
 * Pops the next task (image and its filename) from the input queue.
 * Handles potential queue errors and checks for termination signals (images with zero width and height). Frees resources associated with termination signals or failed pops.
 *
 * @param input_q Pointer to the input queue.
 * @param filename_ptr Pointer to a char pointer where the filename associated with the image will be stored (memory allocated by queue).
 * @param file_count Total expected file count (potentially used by queue logic).
 * @param written_files_ptr Pointer to counter for processed files (potentially used by queue logic).
 *
 * @return Pointer to the bmp_img task, or NULL if queue is empty/error/termination signal.
 */
static bmp_img *worker_get_task(struct img_queue *input_q, char **filename_ptr, int file_count, size_t *written_files_ptr, const char *mode)
{
	bmp_img *img = queue_pop(input_q, filename_ptr, file_count, written_files_ptr, mode);

	if (!img) {
		log_info("Worker Info: queue_pop returned NULL (end of queue or error).");
		if (*filename_ptr) {
			free(*filename_ptr);
			*filename_ptr = NULL;
		}
		return NULL;
	}

	if (img->img_header.biWidth == 0 && img->img_header.biHeight == 0) {
		log_debug("Worker: Received termination signal.");
		free(img);
		if (*filename_ptr) {
			free(*filename_ptr);
			*filename_ptr = NULL;
		}
		return NULL;
	}

	log_debug("Worker: Popped image (w:%u, h:%u) file: %s", img->img_header.biWidth, img->img_header.biHeight, (*filename_ptr ? *filename_ptr : "N/A"));
	return img;
}

/**
 * Allocates and initializes resources (result image, thread specification, image dimensions, image specification) needed for processing one image.
 * Handles cleanup of partially allocated resources on failure.
 *
 * @param input_img The input image popped from the queue.
 * @param pargs Pointer to the program arguments structure.
 * @param filters Pointer to the filter mix structure.
 *
 * @return Pointer to an initialized thread_spec structure containing all necessary data for processing, or NULL on allocation failure.
 */
static struct thread_spec *worker_allocate_resources(bmp_img *input_img, struct p_args *pargs, struct filter_mix *filters)
{
	bmp_img *img_result = NULL;
	struct thread_spec *th_spec = NULL;
	struct img_dim *dim = NULL;
	struct img_spec *img_spec = NULL;

	img_result = malloc(sizeof(bmp_img));
	if (!img_result) {
		log_error("Worker Error: Result image allocation failed");
		return NULL;
	}
	bmp_img_init_df(img_result, input_img->img_header.biWidth, input_img->img_header.biHeight);

	th_spec = init_thread_spec(pargs, filters);
	if (!th_spec) {
		log_error("Worker Error: thread_spec allocation failed");
		bmp_img_free(img_result);
		free(img_result);
		return NULL;
	}

	dim = init_dimensions(input_img->img_header.biWidth, input_img->img_header.biHeight);
	if (!dim) {
		log_error("Worker Error: init_dimensions failed");
		free(th_spec);
		bmp_img_free(img_result);
		free(img_result);
		return NULL;
	}

	img_spec = init_img_spec(input_img, img_result);
	if (!img_spec) {
		log_error("Worker Error: init_img_spec failed");
		free(dim);
		free(th_spec);
		bmp_img_free(img_result);
		free(img_result);
		return NULL;
	}

	th_spec->dim = dim;
	th_spec->img = img_spec;

	return th_spec;
}

/**
 * Processes the image contained within the thread_spec structure according to the compute mode specified in pargs.
 * Uses a local mutex to manage access to block counters if needed by processing functions.
 *
 * @param th_spec The thread specification structure containing image data, dimensions, etc.
 * @param pargs Pointer to the program arguments structure containing compute mode, block size.
 * @param filters Pointer to the filter mix structure.
 *
 * @return 0 on successful processing of the entire image, < 0 on error.
 */
static int worker_process_image(struct thread_spec *th_spec, struct p_args *pargs, struct filter_mix *filters)
{
	uint16_t next_x_block_local = 0;
	uint16_t next_y_block_local = 0;
	int process_status = 0;
	pthread_mutex_t local_xy_mutex = PTHREAD_MUTEX_INITIALIZER;

	while (1) {
		process_status = 0;

		switch ((enum compute_mode)pargs->compute_cfg.compute_mode) {
		case BY_ROW:
			process_status = process_by_row(th_spec, &next_x_block_local, pargs->compute_cfg.block_size, &local_xy_mutex);
			break;
		case BY_COLUMN:
			process_status = process_by_column(th_spec, &next_y_block_local, pargs->compute_cfg.block_size, &local_xy_mutex);
			break;
		case BY_PIXEL:
			process_status = process_by_pixel(th_spec, &next_x_block_local, &next_y_block_local, &local_xy_mutex);
			break;
		case BY_GRID:
			process_status = process_by_grid(th_spec, &next_x_block_local, &next_y_block_local, pargs->compute_cfg.block_size, &local_xy_mutex);
			break;
		default:
			log_error("Worker Error: Invalid compute mode %d", pargs->compute_cfg.compute_mode);
			process_status = -1;
			break;
		}

		if (process_status != 0) {
			if (process_status < 0) {
				log_error("Worker Error: Processing function returned error %d", process_status);
			} else {
				log_debug("Worker: Finished processing chunks for this image.");
				process_status = 0;
			}
			break;
		}
		filter_part_computation(th_spec, pargs->compute_cfg.filter_type, filters);
	}

	pthread_mutex_destroy(&local_xy_mutex);
	return process_status;
}

/**
 * Frees resources allocated specifically for processing one image cycle.
 * Does NOT free the output image data, as its ownership was transferred.
 *
 * @param input_img The original input image structure (popped from queue).
 * @param th_spec The thread specification structure holding related resources.
 */
static void worker_cleanup_image_resources(bmp_img *input_img, struct thread_spec *th_spec)
{
	log_debug("Worker: Cleaning up resources for one image cycle.");

	if (input_img) {
		bmp_img_free(input_img);
		free(input_img);
	}

	if (th_spec) {
		if (th_spec->img)
			free(th_spec->img);
		if (th_spec->dim)
			free(th_spec->dim);
		if (th_spec->st_gen_info)
			free(th_spec->st_gen_info);
		free(th_spec);
	}
}

void *worker_thread(void *arg)
{
	struct qthreads_gen_info *qt_info = (struct qthreads_gen_info *)arg;
	bmp_img *img = NULL;
	bmp_img *img_result = NULL;
	struct thread_spec *th_spec = NULL;
	char *filename = NULL;
	double start_time = 0;
	double result_time = 0;
	int process_status = 0;
	const char *mode_str = NULL;

	log_debug("Worker: thread started.");

	mode_str = compute_mode_to_str(qt_info->pargs->compute_cfg.compute_mode);

	while (1) {
		start_time = get_time_in_seconds();

		img = worker_get_task(qt_info->input_q, &filename, qt_info->pargs->files_cfg.file_cnt, &written_files, mode_str);
		if (!img) {
			log_debug("Worker: Exiting loop due to null task from queue.");
			break;
		}

		th_spec = worker_allocate_resources(img, qt_info->pargs, qt_info->filters);
		if (!th_spec) {
			worker_cleanup_image_resources(img, NULL);
			if (filename)
				free(filename);
			continue;
		}
		img_result = th_spec->img->output_img;

		process_status = worker_process_image(th_spec, qt_info->pargs, qt_info->filters);

		if (process_status != 0) {
			log_error("Worker Error: Image processing failed, discarding result.");
			bmp_img_free(img_result);
			free(img_result);
			img_result = NULL;
		} else {
			queue_push(qt_info->output_q, img_result, filename, mode_str);
			log_debug("Worker: Pushed result for '%s' to output queue.", (filename ? filename : "N/A"));
			img_result = NULL;
		}

		result_time = get_time_in_seconds() - start_time;
		if (result_time > 0) {
			qt_write_logs(result_time, WORKER, mode_str);
		}

		worker_cleanup_image_resources(img, th_spec);

		if (process_status != 0 && filename != NULL) {
			log_debug("Worker: Freeing filename for failed processing of %s", filename);
			free(filename); // <<< ОСВОБОЖДАЕМ FILENAME ПРИ ОШИБКЕ ПОСЛЕ ОБРАБОТКИ
			filename = NULL;
		}
	}

	if (filename) {
		log_warn("Worker: Leaking filename on thread exit?");
		free(filename);
	}

	log_debug("Worker: thread finished.");
	return NULL;
}

void *writer_thread(void *arg)
{
	struct qthreads_gen_info *qt_info = (struct qthreads_gen_info *)arg;
	char output_filepath[MAX_PATH_LEN];
	bmp_img *img = NULL;
	char *filename = NULL;
	double start_time = 0;
	double result_time = 0;
	size_t current_wf_local = 0;
	const char *mode_str = NULL;

	log_debug("Writer: thread started. Expecting %d files.", qt_info->pargs->files_cfg.file_cnt);

	mode_str = compute_mode_to_str(qt_info->pargs->compute_cfg.compute_mode);

	while (1) {
		current_wf_local = __atomic_load_n(&written_files, __ATOMIC_ACQUIRE);
		if (current_wf_local >= qt_info->pargs->files_cfg.file_cnt) {
			log_debug("Writer: All expected files (%zu) accounted for. Exiting.", current_wf_local);
			break;
		}

		start_time = get_time_in_seconds();

		img = queue_pop(qt_info->output_q, &filename, qt_info->pargs->files_cfg.file_cnt, &written_files, mode_str);

		if (!img) {
			log_info("Writer: queue_pop returned NULL. Assuming end of tasks.");
			break;
		}

		if (img->img_header.biWidth == 0 && img->img_header.biHeight == 0) {
			log_warn("Writer: Received unexpected termination signal on output queue.");
			free(img);
			if (filename)
				free(filename);
			continue;
		}

		if (!filename) {
			log_error("Writer Error: Received image from output queue without a filename!");
			bmp_img_free(img);
			free(img);
			continue;
		}

		if (qt_info->pargs->files_cfg.output_filename && strlen(qt_info->pargs->files_cfg.output_filename) > 0) {
			snprintf(output_filepath, sizeof(output_filepath), "test-img/qmt_out_%s_%s", qt_info->pargs->files_cfg.output_filename, filename);
		} else {
			snprintf(output_filepath, sizeof(output_filepath), "test-img/qmt_out_%s", filename);
		}

		if (bmp_img_write(img, output_filepath) != 0) {
			log_error("Writer Error: Failed to write image to '%s'", output_filepath);
		} else {
			current_wf_local = __atomic_add_fetch(&written_files, 1, __ATOMIC_RELEASE);
			log_info("Writer: Successfully wrote '%s' (file %zu/%d)", output_filepath, current_wf_local, qt_info->pargs->files_cfg.file_cnt);

			result_time = get_time_in_seconds() - start_time;
			if (result_time > 0)
				qt_write_logs(result_time, WRITER, mode_str);
		}

		bmp_img_free(img);
		free(img);
		free(filename);
		filename = NULL;
		img = NULL;

		if (current_wf_local >= qt_info->pargs->files_cfg.file_cnt) {
			log_debug("Writer: Reached expected file count (%zu). Exiting.", current_wf_local);
			break;
		}
	}

	if (filename)
		free(filename);

	log_debug("Writer: thread finished.");
	return NULL;
}
