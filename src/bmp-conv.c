// SPDX-License-Identifier: GPL-3.0-or-later

#include "../libbmp/libbmp.h"
#include "../logger/log.h"
#include "utils/utils.h"
#include "mt-mode/exec.h" // for execute_mt_computation
#include "st-mode/exec.h" // for execute_st_computation
#include "qmt-mode/exec.h" // for queue-mode functions
#include "utils/filters.h" // for init_filters, free_filters, struct filter_mix
#include "qmt-mode/threads.h" // for qthreads_gen_info
#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>

#ifdef USE_MPI
#include <mpi.h>
#include "mpi-mode/exec.h"
#endif

#define LOG_FILE_PATH "tests/timing-results.dat"

// Global pointer to parsed arguments. Consider passing this instead of using global.
struct p_args *args = NULL;

/**
 * Parses command-line arguments provided via `argc` and `argv`. Initializes the
 * global `args` structure with defaults, checks for queue mode activation, calls
 * appropriate sub-parsers for mandatory and mode-specific arguments, and validates
 * that all required arguments are present and valid.
 *
 * @return number of worker threads for normal mode (or 1 if queue mode is active) on success, or
 * -1 on any parsing or validation error.
 */
static int parse_args(int argc, char *argv[])
{
	if (argc < 2) {
		log_error("Usage: %s <input.bmp> --filter=<f> --mode=<m> --block=<b> [--threadnum=<N> | -queue-mode --rww=R,W,T] [options...]\n", argv[0]);
		return -1;
	}

	if (!args) {
		log_error("Error: Global args structure not allocated before parse_args.\n");
		return -1;
	}
	initialize_args(args);

	if (argc > 1 && strncmp(argv[1], "-queue-mode", 11) == 0) {
		args->mt_mode = 1;
		argv[1] = "_";
	} else if (argc > 1 && strncmp(argv[1], "-mpi-mode", 10) == 0) {
		args->mt_mode = 2;
		argv[1] = "_";
	}

	if (parse_mandatory_args(argc, argv, args) < 0) {
		log_error("Error parsing mandatory arguments.\n");
		return -1;
	}

	if (args->mt_mode == 1) {
		if (parse_queue_mode_args(argc, argv, args) < 0) {
			log_error("Error parsing queue-mode specific arguments.\n");
			return -1;
		}
	} else {
		if (parse_normal_mode_args(argc, argv, args) < 0) {
			log_error("Error parsing normal-mode specific arguments.\n");
			return -1;
		}
	}

	if (!args->filter_type || args->compute_mode < 0 || args->block_size == 0) {
		log_error("Error: Missing required arguments: --filter, --mode, and --block must be set.\n");
		return -1;
	}
	if (args->mt_mode && args->file_count == 0) {
		log_error("Error: Queue mode requires at least one input filename.\n");
		return -1;
	}
	if (!args->mt_mode && args->file_count != 1) {
		log_error("Error: Normal mode requires exactly one input filename.\n");
		return -1;
	}

	return args->threadnum;
}

/**
 * Executes image filtering in the standard (non-queue) mode, which can be either
 * single-threaded or multi-threaded based on the `threadnum` parameter derived from
 * command-line arguments. It handles reading the single input image, setting up
 * necessary data structures (`img_dim`, `img_spec`), invoking the appropriate
 * computation function (`execute_st_computation` or `execute_mt_computation`),
 * logging the execution time, saving the resulting image, and performing cleanup.
 * Requires the number of threads `threadnum` and an initialized `filter_mix`
 * structure `filters`.
 *
 * @return result time.
 */
static double run_non_queue_mode(int threadnum, struct filter_mix *filters)
{
	bmp_img img = { 0 };
	bmp_img img_result = { 0 };
	struct img_dim *dim = NULL;
	struct img_spec *img_spec = NULL;
	char input_filepath[256];
	char output_filepath[256];
	double result_time = 0;

	if (!args || !args->input_filename[0]) {
		log_error("Error: Missing arguments/input filename for non-queue mode.\n");
		return -1;
	}

	snprintf(input_filepath, sizeof(input_filepath), "test-img/%s", args->input_filename[0]);

	if (bmp_img_read(&img, input_filepath) != 0) {
		log_error("Error: Could not read BMP image '%s'\n", input_filepath);
		goto cleanup;
	}

	dim = init_dimensions(img.img_header.biWidth, img.img_header.biHeight);
	if (!dim) {
		log_error("Error: Failed to initialize dimensions.\n");
		goto cleanup;
	}

	bmp_img_init_df(&img_result, dim->width, dim->height);

	img_spec = init_img_spec(&img, &img_result);
	if (!img_spec) {
		log_error("Error: Failed to initialize image spec.\n");
		goto cleanup;
	}

	assert(threadnum > 0);

	if (threadnum > 1) {
		log_info("Executing multi-threaded computation (%d threads)...", threadnum);
		result_time = execute_mt_computation(threadnum, dim, img_spec, args, filters);
	} else {
		log_info("Executing single-threaded computation...");
		result_time = execute_st_computation(dim, img_spec, args, filters);
	}

	if (result_time <= 0) {
		log_error("Error: Computation execution failed or returned non-positive time (%.6f).\n", result_time);
		goto cleanup;
	}

	save_result_image(output_filepath, sizeof(output_filepath), threadnum, &img_result, args);

cleanup:
	log_debug("Cleaning up non-queue mode resources...");
	if (img_spec) {
		free(img_spec);
	}
	bmp_img_free(&img_result);
	free(dim);
	bmp_img_free(&img);

	return result_time;
}

/**
 * Executes image filtering using the queue-based multi-threaded pipeline model,
 * involving reader, worker, and writer threads. This function orchestrates the
 * process by allocating necessary shared resources (queues, thread info structures,
 * barrier), linking the provided initialized `filter_mix` structure `filters`,
 * creating and launching the different types of threads, waiting for their completion,
 * measuring the total elapsed time, and finally freeing the allocated shared resources.
 *
 * @return total execution time in seconds, or 0.0 if a critical error occurs
 * during resource allocation.
 */
static double run_queue_mode(struct filter_mix *filters)
{
	double start_time, end_time;
	struct img_queue input_queue, output_queue;
	struct qthreads_gen_info *qt_info = NULL;

	log_info("Executing queue-based computation...");

	qt_info = malloc(sizeof(struct qthreads_gen_info));
	if (!qt_info) {
		log_error("Error: Failed to allocate memory for qthreads_gen_info.\n");
		return 0.0;
	}

	if (allocate_qthread_resources(qt_info, args, &input_queue, &output_queue) != 0) {
		free(qt_info);
		return 0.0;
	}
	qt_info->filters = filters;

	start_time = get_time_in_seconds();

	create_qthreads(qt_info);

	join_qthreads(qt_info);

	end_time = get_time_in_seconds();

	free_qthread_resources(qt_info);

	log_info("Queue mode finished in %.6f seconds.", end_time - start_time);
	return end_time - start_time;
}

/**
 * The main entry point of the image filtering application. It orchestrates the
 * overall execution flow: allocates memory for storing parsed arguments, calls
 * the argument parsing routine, allocates and initializes the required filter
 * structures, determines the execution mode (queue-based or standard) based on
 * parsed arguments, invokes the corresponding execution function (`run_queue_mode`
 * or `run_non_queue_mode`), logs the final results if applicable, and performs
 * final cleanup of allocated resources (arguments and filters).
 *
 * @return 0 on successful completion, -1 on memory allocation errors, or -2 on argument
 * parsing errors.
 */
int main(int argc, char *argv[])
{
	double result_time = 0;
	int threadnum = 0;
	struct filter_mix *filters = NULL;
	int return_code = 0;

	log_set_quiet(true); // set false for default usage, true - for benchmarking.
	log_set_level(LOG_TRACE);

	args = malloc(sizeof(struct p_args));
	if (!args) {
		log_error("Fatal Error: Cannot allocate args structure.\n");
		return -1;
	}

	threadnum = parse_args(argc, argv);
	if (threadnum < 0) {
		log_error("Error: Argument parsing failed.\n");
		free(args);
		return -2;
	}

	filters = malloc(sizeof(struct filter_mix));
	if (!filters) {
		log_error("Fatal Error: Cannot allocate filter_mix structure.\n");
		free(args);
		return -1;
	}
	init_filters(filters);

	if (!args->mt_mode) {
		result_time = run_non_queue_mode(threadnum, filters);
	} else if (args->mt_mode == 1) {
		result_time = run_queue_mode(filters);
	}
#ifdef USE_MPI
	else if (args->mt_mode == 2) {
		int rank = 0;
		int size = 0;
		/**
		 * Even though "The MPI standard does not say what a program can do before an MPI_INIT or after an MPI_FINALIZE. In the MPICH implementation, you should do as little as possible. In particular, avoid anything that changes the external state of the program, such as opening files, reading standard input or writing to standard output." - it should be fine
			*/
		MPI_Init(&argc, &argv);

		MPI_Comm_rank(MPI_COMM_WORLD, &rank);
		MPI_Comm_size(MPI_COMM_WORLD, &size);

		log_info("SIZE = %d", size);
		// immediatly jumps to execute_...
		// (run_.._mode phase with initialisation is included in there, bc it depends on computation type)
		result_time = execute_mpi_computation(size, rank, args, filters);

		MPI_Finalize();
	}
#endif
	else { // should be unreachable, just a plug
		log_error("Error: invalid mode = %d", args->mt_mode);
	}

	st_write_logs(args, result_time);

	free_filters(filters);
	free(filters);
	free(args);

	return return_code;
}
