// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once 

#include "../utils/threads-general.h"
#include "mpi-types.h"

/**
 * This function represents the data distribution phase. 
 * It performs an `MPI_Scatterv` operation. Rank 0 acts as the sender,
 * distributing chunks of the pre-packed `global_send_buffer` (prepared in a prior phase)
 * to all processes according to the sizes and displacements specified in `comm_arrays`.
 * Each process receives its designated chunk (including halo rows) into its
 * freshly allocated `local_data->input_pixels` buffer.
 *
 * @param ctx - pointer to the MPI context structure
 * @param comm_data - pointer to the image communication geometry structure
 * @param local_data - pointer to the local data structure.
* @param global_send_buffer - pointer to the contiguous buffer on rank 0 containing the packed image data ready for scattering. 
 * @param comm_arrays - pointer to the structure containing communication arrays
 * 
 * @return 0 on success, -1 on error .
 */
int mpi_phase_scatter_data(const struct mpi_context *ctx, const struct img_comm_data *comm_data, struct mpi_local_data *local_data, unsigned char *global_send_buffer, const struct mpi_comm_arr *comm_arrays);

/**
 * Each rank sends its computed data from `local_data->output_pixels` via `MPI_Gatherv`.
 * Rank 0 allocates `global_recv_buffer`, receives data using `comm_arrays` specs,
 * unpacks into `img_data->img_result` via `mpi_rank0_unpack_data_from_gather`,
 * and frees `global_recv_buffer`.
 *
 * @param ctx - MPI context 
 * @param comm_data - image communication geometry 
 * @param local_data - contains processed data in `output_pixels` to send
 * @param comm_arrays - gatherv receive counts/displacements 
 * @param img_data - final image destination 
 *
 * @return 0 on success, -1 on error
 */
int mpi_phase_gather_data(const struct mpi_context *ctx, const struct img_comm_data *comm_data, const struct mpi_local_data *local_data, const struct mpi_comm_arr *comm_arrays, struct img_spec *img_data);

/**
 * This function is typically called after rank 0 has determined the image
 * dimensions (e.g., by reading the image file header). It uses `MPI_Bcast`
 * twice to send the width and height values from rank 0's `comm_data->dim`
 * structure to all processes in the `MPI_COMM_WORLD` communicator.
 *
 * @param comm_data Pointer to the image communication geometry structure.
 * Rank 0 reads `dim->width` and `dim->height` from this structure.
 *
 * @note The function returns void; MPI errors during broadcast are typically fatal
 * and might lead to hangs or termination if not handled by MPI itself or by setting MPI error handlers.
 */
void mpi_broadcast_metadata(struct img_comm_data *comm_data); 

