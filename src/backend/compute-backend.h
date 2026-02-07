// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "utils/threads-general.h"
#include "utils/filters.h"
#include "utils/args-parse.h"




struct compute_backend;

/**
 * Compute Backend operations interface.
 * Every compute backend (CPU, GPU, MPI) implements this interface.
 */

struct compute_backend_ops {
	/**
	 * Initializes the compute backend with given arguments.
	 * 
	 * @return 0 on success, negative value on error.
	 */
	int (*init)(struct compute_backend *backend, struct p_args *args);

	/**
	 * Processes an image using the specified filter and compute mode.
	 * 
	 * @return Processing time in seconds, or 0.0 on error.
	 */
	double (*process_image)(struct compute_backend *backend);

	/**
	 * Cleans up resources associated with the compute backend.
	 */
	void (*cleanup)(struct compute_backend *backend);

	/**
	 * Returns the type of the compute backend.
	 * 
	 * @return Backend type enum value.
	 */
	enum conv_backend (*get_type)(void);

	/**
	 * Returns the name of the compute backend.
	 * 
	 * @return Constant string with backend name.
	 */
	const char *(*get_name)(void);
};

/**
 * Compute Backend structure.
 * Contains operations and backend-specific context data.
 */
struct compute_backend {
	enum conv_backend backend; // for fast access
	const struct compute_backend_ops *ops;
	
	void *backend_data;  // Backend-specific context data (threads, GPU context, MPI comm, etc.)
	struct p_args *args;
	struct filter_mix *filters;
};

/**
 * Creates a compute backend of the specified type.
 * 
 * @return Pointer to created compute_backend structure, or NULL on error.
 */
struct compute_backend *compute_backend_create(struct p_args *args, struct filter_mix *filters);

/**
 * Destroys a compute backend and frees associated resources.
 */
double compute_backend_run(struct compute_backend *backend);

void compute_backend_destroy(struct compute_backend *backend);

