// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mpi-types.h"
#include <stdint.h>
#include "utils/filters.h"

/**
 * Determines the required halo size based on the selected filter type.
 *
 * The halo size is typically half the filter kernel size (integer division).
 * For the median filter ('mm'), a fixed size is assumed if not provided otherwise.
 *
 * @param filter_type A string representing the chosen filter (e.g., "mb", "gb", "mm").
 * @param filters A pointer to the structure containing all initialized filter kernels.
 * @return The required halo size (padding) for the given filter, or 0 if the filter is invalid or requires no halo.
 */
uint8_t get_halo_size(const char *filter_type, const struct filter_mix *filters);

// simply frees the mpi_comm_arr struct data
void free_comm_arr(struct mpi_comm_arr comm_arrays);

/**
 * Frees the mpi_comm_arr and mpi_local_data structuresp
 *
 * @param ctx - process-specific mpi_context
 * @param local_data - local copy of input pixels and output pixels
 * @param comm_arrays - arrays of distribution data description (in terms of packed arrays)
 *
 * @return 0 on success, -1 on error
 */
void mpi_phase_cleanup_resources(const struct mpi_context *ctx, struct mpi_local_data *local_data, struct mpi_comm_arr *comm_arrays);

/**
 * Allocates mpi_local_data structure by relying upon img_comm_data (num of rows).
 *
 * @param ctx - process-specific mpi_context
 * @param comm_data - process-specific img_comm_data (that stores the resulst of distr calc)
 * @return 0 on success, -1 on error
 */
int8_t mpi_allocate_local_buffers(const struct mpi_context *ctx, const struct img_comm_data *comm_data, struct mpi_local_data *local_data);

/**
 * Calculates the distribution based on row/column method and mpi process context.
 *
 * @param ctx - process-specific mpi_context
 * @param comm_data - process-specific img_comm_data (that stores the resulst of distr calc)
 *
 */
void mpi_calculate_row_distribution(const struct mpi_context *ctx, struct img_comm_data *comm_data);
void mpi_calculate_column_distribution(const struct mpi_context *ctx, struct img_comm_data *comm_data);

/**
 * Sets up the comm_data and comm_data for each process.
 * More specifically - calculates the distribution range for each process and sets all the auxilialry info.
 * See mpi-types.h for better type description.
 *
 * @param ctx - process-specific mpi_context
 * @param comm_data - process-specific img_comm_data (that stores the resulst of distr calc)
 * @param comm_arrays - distribution info (num of rows, ...)
 *
 * @return 0 on success, -1 on error
 */
int8_t mpi_setup_scatter_gather_row_arrays(const struct mpi_context *ctx, const struct img_comm_data *comm_data, struct mpi_comm_arr *comm_arrays);
