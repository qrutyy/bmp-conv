// SPDX-License-Identifier: GPL-3.0-or-later

#include "mpi-types.h"

/**
 * This function represents the data distribution phase. 
 * It performs an `MPI_Scatterv` operation. Rank 0 acts as the sender,
 * distributing chunks of the pre-packed `global_send_buffer` (prepared in a prior phase)
 * to all processes according to the sizes and displacements specified in `comm_arrays`.
 * Each process receives its designated chunk (including halo rows) into its
 * freshly allocated `local_data->input_pixels` buffer.
 *
 * @param ctx Pointer to the MPI context structure (rank, size).
 * @param comm_data Pointer to the image communication geometry structure (needed for buffer allocation size).
 * @param local_data Pointer to the local data structure. Its `input_pixels` and `output_pixels` buffers will be allocated 
 * by this function (via mpi_allocate_local_buffers), and `input_pixels` will serve as the receive buffer for MPI_Scatterv.
 * @param global_send_buffer Pointer to the contiguous buffer on rank 0 containing the packed image data ready for scattering. 
 * This buffer is freed by rank 0 within this function. Ignored by non-zero ranks.
 * @param comm_arrays Pointer to the structure containing communication arrays (`sendcounts`, `displs`) used by rank 0 in the MPI_Scatterv call.
 *
 * @return 0 on success, -1 on error (e.g., buffer allocation failure, MPI_Scatterv error).
 */
int mpi_phase_scatter_data(const struct mpi_context *ctx, const struct img_comm_data *comm_data, struct mpi_local_data *local_data, unsigned char *global_send_buffer, const struct mpi_comm_arr *comm_arrays);

/**
 *
 * This function represents the result collection phase. 
 * Rank 0 allocates a sufficiently large buffer (`global_recv_buffer`) to receive
 * all the chunks according to the sizes and displacements specified in `comm_arrays`.
 * After the `MPI_Gatherv` completes, rank 0 calls `mpi_rank0_unpack_data_from_gather`
 * to copy the received chunks from the `global_recv_buffer` into their correct
 * positions within the final result image structure (`img_data->img_result`).
 * The temporary `global_recv_buffer` on rank 0 is freed after unpacking.
 *
 * @param ctx Pointer to the MPI context structure (rank, size).
 * @param comm_data Pointer to the image communication geometry structure (needed for send size calculation and unpacking).
 * @param local_data Pointer to the local data structure, containing the processed data in `output_pixels` to be sent.
 * @param comm_arrays Pointer to the structure containing communication arrays (`recvcounts`, `recvdispls`)
 * used by rank 0 in the MPI_Gatherv call and for unpacking.
 * @param img_data Pointer to the main image specification structure. On rank 0, the gathered and unpacked
 * result will be placed into `img_data->img_result->img_pixels`. Ignored by non-zero ranks.
 *
 * @return 0 on success, -1 on error (e.g., buffer allocation failure on rank 0, MPI_Gatherv error, unpacking error).
 */
int mpi_phase_gather_data(const struct mpi_context *ctx, const struct img_comm_data *comm_data, const struct mpi_local_data *local_data, const struct mpi_comm_arr *comm_arrays, struct img_spec *img_data);

/**
 *
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

