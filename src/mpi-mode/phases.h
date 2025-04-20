// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdint.h>
#include "mpi-types.h"
#include "../utils/args-parse.h"
/**
 * Initialisation phase function. It sets up the root process (rank0), broadcasts the general metadata (height and width) 
 * and calculates the distribution information.
 * 
 * @param ctx - mpi_context with general size of mpi and current rank 
 * @param comm_data - img_comm_data that incapsulates process-specific distribution data
 * + other known params
 *
 * @return 0 on success, other way -1
 */
int8_t mpi_phase_initialize(const struct mpi_context *ctx, const struct p_args *args, struct img_spec *img_data, struct img_comm_data *comm_data, double *start_time);

/**
 * Setups the pre-computation data. Initialises gather row arrays. 
 * Calculates the row-distribution border for each process. 
 * Finally - packs the data (see. mpi_rank0_pack_data_for_scatter) 
 * 
 * @param ctx - process-specific mpi_context 
 * @param comm_data - pointer to root0 img_comm_data structure
 * @param comm_arrays - pointer to structure, that bundles together the arrays required for variable-count MPI collective communication operations, specifically those like MPI_Scatterv and MPI_Gatherv. 
 * @param global_send_buffer - continuous buffer
 */
int8_t mpi_phase_prepare_comm(const struct mpi_context *ctx, const struct img_comm_data *comm_data, const struct img_spec *img_data, struct mpi_comm_arr *comm_arrays,
				     unsigned char **global_send_buffer);

// Just verifies the input and calls mpi_process_local_region
void mpi_phase_process_region(const struct mpi_context *ctx, const struct img_comm_data *comm_data, const struct mpi_local_data *local_data, const struct p_args *args,
				     const struct filter_mix *filters);

// (see `mpi_rank0_finalize_and_save` doc)
double mpi_phase_finalize_and_broadcast(const struct mpi_context *ctx, double start_time, struct img_spec *img_data, const struct p_args *args);

