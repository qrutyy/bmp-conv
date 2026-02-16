// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "utils/filters.h"
#include "utils/args-parse.h"
#include "../utils/mpi-types.h"
#include <stdint.h>

/**
 * Processes the local image region assigned to the current MPI rank.
 *
 * @param local_data - pointer to the structure holding local input/output pixel buffers (unsigned char*).
 * @param comm_data - pointer to the structure holding MPI communication geometry for this rank.
 * @param filter_type - string identifier for the desired filter (e.g., "mb", "mm", "sh").
 * @param filters - pointer to the structure containing pre-initialized filter kernels.
 * @param halo_size - the size of the halo (ghost rows) included in the input buffer.
 */
void mpi_compute_local_region(const struct mpi_local_data *local_data, const struct img_comm_data *comm_data, const struct p_args *args, const struct filter_mix *filters,
			      const struct mpi_context *ctx);
