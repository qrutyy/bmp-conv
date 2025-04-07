// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdlib.h>
#include <string.h>
#include "../utils/utils.h"
#include "../utils/threads-general.h"
#include "exec.h"

/**
  *  Function that set ups the necessary for single threaded computation structures. After - starts the computation by directly calling filter_part_computation. 
  *
  *  Returns the time spent for computation.
  */
double execute_st_computation(struct img_dim *dim, struct img_spec *img_spec, struct p_args *args, void *filters)
{
	double start_time, end_time;
	struct thread_spec *spec = init_thread_spec(args, filters);

	if (!spec) {
		fprintf(stderr, "Error: memory allocation error for thread_spec\n");
		return 0;
	}

	spec->dim = dim;
	spec->img = img_spec;
    // Single thread handles the whole image
	spec->start_row = 0; 
	spec->end_row = dim->height;
	spec->start_column = 0; 
	spec->end_column = dim->width;

	start_time = get_time_in_seconds();

	// Directly call the computation function (no splitting is needed)
	filter_part_computation(spec, args->filter_type, filters);

	end_time = get_time_in_seconds();

	free(spec);

	return end_time - start_time;
}


