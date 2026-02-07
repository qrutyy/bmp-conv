// SPDX-License-Identifier: GPL-3.0-or-later

#include "cpu-backend.h"
#include "../compute-backend.h"
#include "utils/threads-general.h"
#include "logger/log.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "utils/utils.h"
#include "st/st_exec.h"
#include "mt/mt_exec.h"
#include "qmt/qmt_exec.h"
#include "qmt/qmt_threads.h"

/**
 * CPU-specific backend data.
 * Contains thread management information.
 */
struct cpu_backend_data {
	int thread_count;
};

int cpu_verify_args(struct p_args *args)
{
	if (!args->compute_cfg.filter_type || args->compute_cfg.block_size == 0) {
		log_error("Error: Missing required arguments: --filter and --block must be set.\n");
		return -1;
	}

	if (args->compute_cfg.compute_mode < 1) {
		log_warn("Warn: --mode is required for CPU backend mode, setting BY_ROW.\n");
		args->compute_cfg.compute_mode = CONV_COMPUTE_BY_ROW;
	}
	if (args->compute_cfg.queue == CONV_QUEUE_ENABLED && args->files_cfg.file_cnt <= 2) {
		log_error("Error: Queue mode requires at least 3 input filename.\n");
		return -1;
	}
	if (args->compute_cfg.queue == CONV_QUEUE_DISABLED && args->files_cfg.file_cnt != 1) {
		log_error("Error: Normal mode requires exactly one input filename.\n");
		return -1;
	}

	return 0;
}

static int cpu_init(struct compute_backend *backend, struct p_args *args)
{
	struct cpu_backend_data *data = NULL;
	int rc;

	if (!backend || !args) {
		log_error("Error: NULL parameter in cpu_init\n");
		return -1;
	}

	data = malloc(sizeof(struct cpu_backend_data));
	if (!data) {
		log_error("Error: Failed to allocate memory for cpu_backend_data\n");
		return -1;
	}

	rc = cpu_verify_args(args);
	if (rc) {
		free(data);
		return rc;
	}

	// Determine thread count based on arguments
	if (args->compute_ctx.threadnum > 0) {
		data->thread_count = args->compute_ctx.threadnum;
	} else {
		data->thread_count = 1;
	}

	backend->backend_data = data;
	
	log_debug("CPU Backend: Initialized with %d threads\n", data->thread_count);
	return 0;
}

static double cpu_process_non_queue_mode(struct compute_backend *backend)
{
	struct p_args *args = backend->args;
	struct filter_mix *filters = backend->filters;
	struct cpu_backend_data *data = (struct cpu_backend_data *)backend->backend_data;
	int threadnum = data->thread_count;
	
	struct img_spec *img_spec = NULL;
	char output_filepath[256];
	double result_time = 0;

	assert(threadnum > 0);

	img_spec = setup_img_spec(args);
	if (!img_spec)
		goto cleanup;

	if (threadnum > 1) {
		log_info("Executing multi-threaded computation (%d threads)...", threadnum);
		result_time = execute_mt_computation(threadnum, img_spec, args, filters);
	} else {
		log_info("Executing single-threaded computation...");
		result_time = execute_st_computation(img_spec, args, filters);
	}

	if (result_time <= 0) {
		log_error("Error: Computation execution failed or returned non-positive time (%.6f).\n", result_time);
		goto cleanup;
	}

	save_result_image(output_filepath, sizeof(output_filepath), threadnum, img_spec->output, args);

cleanup:
	log_debug("Cleaning up non-queue mode resources...");

	if (img_spec) {
		if (img_spec->output) bmp_img_free(img_spec->output);
		if (img_spec->input) bmp_img_free(img_spec->input);
		if (img_spec->dim) free(img_spec->dim);
		free(img_spec);
	}

	return result_time;
}

static double cpu_process_queue_mode(struct compute_backend *backend)
{
	struct p_args *args = backend->args;
	struct filter_mix *filters = backend->filters;
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

static double cpu_process_image(struct compute_backend *backend)
{
	if (backend->args->compute_cfg.queue == CONV_QUEUE_DISABLED) {
		return cpu_process_non_queue_mode(backend);
	} else {
		return cpu_process_queue_mode(backend);
	}
}

static void cpu_cleanup(struct compute_backend *backend)
{
	if (!backend)
		return;

	if (backend->backend_data) {
		free(backend->backend_data);
		backend->backend_data = NULL;
	}
}

static enum conv_backend cpu_get_type(void)
{
	return CONV_BACKEND_CPU;
}

static const char *cpu_get_name(void)
{
	return "CPU";
}

const struct compute_backend_ops cpu_backend_ops = {
	.init = cpu_init,
	.process_image = cpu_process_image,
	.cleanup = cpu_cleanup,
	.get_type = cpu_get_type,
	.get_name = cpu_get_name
};
