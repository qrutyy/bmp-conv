// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../logger/log.h"
#include "../mt-mode/compute.h"
#include "../utils/args-parse.h"
#include "../utils/filters.h"
#include "row-compute.h"
#include <mpi.h>

double execute_mpi_computation(uint8_t size, uint8_t rank, struct p_args *compute_args, struct filter_mix *filters) {
	double total_time = 0;

    switch ((enum compute_mode)compute_args->compute_mode) {
        case BY_ROW:
			total_time = mpi_process_by_rows(rank, size, compute_args, filters);
            break;
        case BY_GRID:
		break;
        case BY_PIXEL:
		break;
        case BY_COLUMN:
		break;
        default:
            if (size == 0) {
                log_error("Error: Invalid compute_mode (%d) for MPI.", compute_args->compute_mode);
            }
            MPI_Abort(MPI_COMM_WORLD, 1); // End all processes in case of error 
            return -1.0;
    }
	return total_time;
}












