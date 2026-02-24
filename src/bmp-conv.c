// SPDX-License-Identifier: GPL-3.0-or-later

#include "libbmp/libbmp.h"
#include "logger/log.h"
#include "utils/utils.h"
#include "utils/args-parse.h"
#include "utils/filters.h"
#include "utils/cli.h"
#include "utils/modes.h"
#include "backend/compute-backend.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

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

	log_set_quiet(!DEBUG_MODE_IS_ON);
	log_set_level(DEBUG_MODE_IS_ON ? LOG_TRACE : LOG_ERROR);

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

	cli_st_display_init(args, backend);

	result_time = compute_backend_run(backend);

	cli_st_display_finish(result_time);

	if (result_time > 0) {
		/* TODO: make it more accurate */
		int rank = (backend->ops->get_logging_rank ? backend->ops->get_logging_rank(backend) : 0);
		if (rank == 0)
			write_logs(args, result_time, backend->backend);
	}

	compute_backend_destroy(backend);

	free_filters(filters);
	free(filters);
	if (args->files_cfg.input_filename)
		free(args->files_cfg.input_filename);
	free(args);

	return 0;
}
