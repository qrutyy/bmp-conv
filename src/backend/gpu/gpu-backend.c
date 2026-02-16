// SPDX-License-Identifier: GPL-3.0-or-later

#include "gpu-backend.h"
#include "../compute-backend.h"
#include "utils/threads-general.h"
#include "logger/log.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "utils/utils.h"
#include "core/mw-exec.h"

int gpu_verify_args(struct p_args *args)
{
	if (!args->compute_cfg.filter_type || args->compute_cfg.block_size == 0) {
		log_error("Error: Missing required arguments: --filter and --block must be set.\n");
		return -1;
	}

	if (args->compute_cfg.compute_mode < 1) {
		log_warn("Warn: --mode is required for gpu backend mode, setting BY_ROW.\n");
		args->compute_cfg.compute_mode = CONV_COMPUTE_BY_ROW;
	}
	/*
	if (args->compute_cfg.queue == CONV_QUEUE_ENABLED && args->files_cfg.file_cnt <= 2) {
		log_error("Error: Queue mode requires at least 3 input filename.\n");
		return -1;
	}
	*/
	if (args->compute_cfg.queue == CONV_QUEUE_ENABLED) {
		log_error("Error: Queued mode isn't supported\n");
		return -1;
	}

	return 0;
}

static int gpu_init(struct compute_backend *backend, struct p_args *args)
{
	struct gpu_backend_data *data = NULL;
	int rc;

	if (!backend || !args) {
		log_error("Error: NULL parameter in gpu_init\n");
		return -1;
	}

	rc = gpu_verify_args(args);
	if (rc) {
		free(data);
		return rc;
	}

	backend->backend_data = data;
	
	return 0;
}

static double gpu_process_non_queue_mode(struct compute_backend *backend)
{
	struct p_args *args = backend->args;
	struct filter_mix *filters = backend->filters;
	struct img_spec *img_spec = NULL;
	char output_filepath[256];
	double result_time = 0;

	img_spec = setup_img_spec(args);
	if (!img_spec)
		goto cleanup;

	/* i really cant get away with this eneven compile option checks (MPI in compute_backend), 
	 * but mpi cant really be incapsulated into cpu mode 
	 * UPD: ok, ive checked, thats real, ill try to unite them asap */
#ifdef USE_OPENCL
	result_time = opencl_execute_basic_computation(img_spec, args, filters);
#else
	log_error("Error: use OpenCL GPU backend requested but USE_OpenCL is not defined (check build logs)\n");
	free(backend);
	return -1;
#endif

	if (result_time <= 0) {
		log_error("Error: Computation execution failed or returned non-positive time (%.6f).\n", result_time);
		goto cleanup;
	}

	/* TODO: get worker_item count */
	save_result_image(output_filepath, sizeof(output_filepath), 0, img_spec->output, args);

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

static double gpu_process_queue_mode(struct compute_backend *backend)
{
	(void)backend;
	return 0;
}

static double gpu_process_image(struct compute_backend *backend)
{
	if (backend->args->compute_cfg.queue == CONV_QUEUE_DISABLED) {
		return gpu_process_non_queue_mode(backend);
	} else {
		return gpu_process_queue_mode(backend);
	}
}

static void gpu_cleanup(struct compute_backend *backend)
{
	if (!backend)
		return;

	if (backend->backend_data) {
		free(backend->backend_data);
		backend->backend_data = NULL;
	}
}

static enum conv_backend gpu_get_type(void)
{
	return CONV_BACKEND_GPU;
}

static const char *gpu_get_name(void)
{
	return "gpu";
}

const struct compute_backend_ops gpu_backend_ops = {
	.init = gpu_init,
	.process_image = gpu_process_image,
	.cleanup = gpu_cleanup,
	.get_type = gpu_get_type,
	.get_name = gpu_get_name
};
