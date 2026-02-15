// SPDX-License-Identifier: GPL-3.0-or-later

#include "libbmp/libbmp.h"
#include "logger/log.h"
#include "utils/utils.h"
#include "utils/args-parse.h"
#include "utils/filters.h"
#include "backend/compute-backend.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define LOG_FILE_PATH "tests/timing-results.dat"

// Global pointer to parsed arguments. Consider passing this instead of using global.
struct p_args *args = NULL;

/**
 * The main entry point of the image filtering application. It orchestrates the
 * overall execution flow: allocates memory for storing parsed arguments, calls
 * the argument parsing routine, allocates and initializes the required filter
 * structures, creates the compute backend, runs the computation, logs results,
 * and performs cleanup.
 *
 * @return 0 on successful completion, -1 on errors.
 */
int main(int argc, char *argv[])
{
	struct compute_backend *backend;
	double result_time = 0;
	struct filter_mix *filters = NULL;
	int rc = 0;

	log_set_quiet(false); // set false for default usage, true - for benchmarking.
	log_set_level(LOG_TRACE);

	args = malloc(sizeof(struct p_args));
	if (!args) {
		log_error("Fatal Error: Cannot allocate args structure.\n");
		return -1;
	}

	rc = parse_args(argc, argv, args);
	if (rc < 0) {
		free(args);
		return -1;
	}
	
	filters = setup_filters(args);
	if (!filters) {
		free(args);
		return -1;
	}

	backend = compute_backend_create(args, filters, &argc, &argv);
	if (!backend) {
		free_filters(filters);
		free(filters);
		free(args);
		return -1;
	}

	result_time = compute_backend_run(backend);

	if (result_time > 0) {
		st_write_logs(args, result_time);
	}

	compute_backend_destroy(backend);

	free_filters(filters);
	free(filters);
	if (args->files_cfg.input_filename)
		free(args->files_cfg.input_filename);
	free(args);

	return 0;
}
