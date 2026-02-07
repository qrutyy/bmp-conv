// SPDX-License-Identifier: GPL-3.0-or-later

#include "compute-backend.h"
#include "cpu/cpu-backend.h"
#include "logger/log.h"
#include <stdlib.h>

#ifdef USE_MPI
#include "mpi/mpi-backend.h"
#endif

struct compute_backend *compute_backend_create(struct p_args *args, struct filter_mix *filters)
{
	enum conv_backend type = args->compute_cfg.backend;
	struct compute_backend *backend = NULL;

	backend = malloc(sizeof(struct compute_backend));
	if (!backend) {
		log_error("Error: Failed to allocate memory for compute_backend\n");
		return NULL;
	}

	backend->args = args;
	backend->filters = filters;
	backend->backend_data = NULL;

	switch (type) {
	case CONV_BACKEND_CPU:
		backend->ops = &cpu_backend_ops;
		break;
	case CONV_BACKEND_GPU:
		log_error("Error: GPU backend not yet implemented\n");
		free(backend);
		return NULL;
	case CONV_BACKEND_MPI: 
#ifdef USE_MPI
		backend->ops = &mpi_backend_ops;
		break;
#else
		log_error("Error: MPI backend requested but USE_MPI is not defined\n");
		free(backend);
		return NULL;
#endif
	default:
		log_error("Error: Unknown compute backend type %d\n", type);
		free(backend);
		return NULL;
	}

	// Initialize the backend
	if (backend->ops->init(backend, args) != 0) {
		log_error("Error: Failed to initialize compute backend\n");
		free(backend);
		return NULL;
	}

	return backend;
}

double compute_backend_run(struct compute_backend *backend)
{
	return backend->ops->process_image(backend);
}

void compute_backend_destroy(struct compute_backend *backend)
{
	if (!backend)
		return;

	// Cleanup backend-specific resources
	if (backend->ops && backend->ops->cleanup) {
		backend->ops->cleanup(backend);
	}

	free(backend);
}
