// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../utils/args-parse.h"
#include "../../libbmp/libbmp.h"
#include <stdint.h>

/**
 * Initialises main data for root process (rank0). Reads the input file, initialises result image structure and comm_data's dimensions.
 * Starts the MPI timer.
 *
 * @param comm_data - pointer to root0 img_comm_data structure (made for saving the dimensions)
 * + other known params
 *
 * @return 0 on success, -1 on error
 */
int8_t mpi_rank0_initialize(struct img_spec *img_data, struct img_comm_data *comm_data, double *start_time, const char *input_filename_base);

/**
 * Finalises the computation by getting 'end_time', saving the image and freeing some allocated data.
 *
 * @param ctx - process-specific mpi_context
 * + other basic args
 *
 * @return total computation time (double)
 */
double mpi_rank0_finalize_and_save(const struct mpi_context *ctx, double start_time, struct img_spec *img_data, const struct p_args *args);

/**
 * Packing data for scatter by reforming the pixels 2d array into a continuous buffer. Made as a preparation for MPI_Scatterv.
 * Determines the specific rows of the original input image (img_data) that need to be sent to that (ith) process, using pre-calculated 'counts' (params->sendcounts) and displacements (params->displs_original).
 * Copyies those rows sequentially into the allocated packed_buffer.
 *
 * @param comm_data - process-specific img_comm_data distr data
 * @param ctx - process-specific mpi_context
 * @param parcked_buffer - continuous buffer
 * + other known params
 *
 * @return 0 on success, -1 on error
 */
int8_t mpi_rank0_pack_data_for_scatter(const struct img_spec *img_data, const struct img_comm_data *comm_data, const struct mpi_context *ctx, const int *sendcounts,
				       const int *displs_original, unsigned char **packed_buffer);

/**
 * Takes the contiguous buffer (gathered_buffer) received from an MPI_Gatherv operation (which contains processed image data chunks from all MPI processes) and unpacks/reassembles it into the final result image structure (img_data->img_result):
 * Calculates the corresponding starting row index (proc_start_row) and number of rows (proc_num_rows) in the final destination image where this data belongs.
 * Sets a pointer (current_unpack_ptr) to the beginning of rank i's data within the gathered_buffer.
 * Copying, row by row, the data from the gathered_buffer (starting at current_unpack_ptr) into the appropriate rows of the destination image.
 *
 * @param gathered_buffer - continuous buffer (from mpi_rank0_pack_data_for_scatter)
 * @param comm_data - pointer to root0 img_comm_data structure
 * @param ctx - process-specific mpi_context
 * @param comm_arrays - pointer to structure, that bundles together the arrays required for variable-count MPI collective communication operations, specifically those like MPI_Scatterv and MPI_Gatherv.
 *
 * @return 0 on success, -1 on error
 */

int8_t mpi_rank0_unpack_data(const unsigned char *gathered_buffer, struct img_spec *img_data, const struct img_comm_data *comm_data, const struct mpi_context *ctx,
			     const struct mpi_comm_arr *comm_arrays);

/**
 * Reinits the buffer by allocating output pixel buffer with transposed dimenstions
 *
 * @param u know...
 *
 * @return 0 on success, -1 on error
 */
int8_t mpi_rank0_reinit_buffer_for_gather(struct img_comm_data *comm_data, struct img_spec *img_data, uint32_t width, uint32_t height);

/**
 * Transposes the image back (pixel array). Updates the dimensions.
 * @param same as in mpi_rank0_unpack_data
 *
 * @return 0 on success, -1 on error
 */
int8_t mpi_rank0_transpose_img_back(struct img_comm_data *comm_data, struct img_spec *img_data, uint32_t width, uint32_t height);
int8_t mpi_rank0_transpose_img(struct img_comm_data *comm_data, struct img_spec *img_data, uint32_t width, uint32_t height);
