// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../logger/log.h"
#include "../../libbmp/libbmp.h"
#include "../utils/threads-general.h"
#include "mpi-types.h"
#include <stdlib.h>

void free_comm_arr(struct mpi_comm_arr comm_arrays) {
	free(comm_arrays.sendcounts); 
	free(comm_arrays.displs);
    free(comm_arrays.recvcounts); 
	free(comm_arrays.recvdispls);
}

void mpi_phase_cleanup_resources(const struct mpi_context *ctx,
                                        struct mpi_local_data *local_data,
                                        struct mpi_comm_arr *comm_arrays)
{
    free(local_data->input_pixels);
    local_data->input_pixels = NULL;
    free(local_data->output_pixels);
    local_data->output_pixels = NULL;

    if (ctx->rank == 0) {
        free(comm_arrays->sendcounts);
        comm_arrays->sendcounts = NULL;
        free(comm_arrays->displs);
        comm_arrays->displs = NULL;
        free(comm_arrays->recvcounts);
        comm_arrays->recvcounts = NULL;
        free(comm_arrays->recvdispls);
        comm_arrays->recvdispls = NULL;
    }
}

int mpi_allocate_local_buffers(const struct mpi_context *ctx, const struct img_comm_data *comm_data, struct mpi_local_data *local_data) {
    size_t input_buf_size = 0;
    size_t output_buf_size = 0;

    input_buf_size = (size_t)comm_data->send_num_rows * comm_data->row_stride_bytes;
    output_buf_size = (size_t)comm_data->my_num_rows * comm_data->row_stride_bytes;

    local_data->input_pixels = (unsigned char *)malloc(input_buf_size > 0 ? input_buf_size : 1);
    local_data->output_pixels = (unsigned char *)malloc(output_buf_size > 0 ? output_buf_size : 1);

    if (!local_data->input_pixels || !local_data->output_pixels) {
        log_error("Rank %d: Failed to allocate local pixel buffers (in: %zu B, out: %zu B).",
                  ctx->rank, input_buf_size, output_buf_size);
        free(local_data->input_pixels);
        free(local_data->output_pixels);
        local_data->input_pixels = local_data->output_pixels = NULL;
        return -1;
    }
    return 0;
}


void mpi_verify_distribution_range(struct img_comm_data *comm_data)
{
    uint32_t send_end_row = 0;
    comm_data->send_start_row = (uint32_t)((int)comm_data->my_start_row - MPI_HALO_SIZE);
    send_end_row = comm_data->my_start_row + comm_data->my_num_rows + MPI_HALO_SIZE;

    if ((int)comm_data->send_start_row < 0) comm_data->send_start_row = 0;
    if (send_end_row > comm_data->dim->height) send_end_row = comm_data->dim->height;

    if (comm_data->send_start_row >= send_end_row) {
         comm_data->send_num_rows = 0;
    } else {
         comm_data->send_num_rows = send_end_row - comm_data->send_start_row;
    }
}

void mpi_calculate_row_distribution(const struct mpi_context *ctx, struct img_comm_data *comm_data) {
	uint32_t rows_per_proc = 0;
	uint32_t remainder_rows = 0;
    uint32_t u_height = comm_data->dim->height;

	if (ctx->size == 0) { // kinda an error
		log_warn("MPI: size in context == %lu\n", ctx->size);
        comm_data->my_start_row = 0;
        comm_data->my_num_rows = (ctx->rank == 0) ? u_height : 0;
        return;
    }

    rows_per_proc = u_height / (uint32_t)ctx->size;
    remainder_rows = u_height % (uint32_t)ctx->size;

    comm_data->my_start_row = (uint32_t)ctx->rank * rows_per_proc + ((uint32_t)ctx->rank < remainder_rows ? (uint32_t)ctx->rank : remainder_rows);
    comm_data->my_num_rows = rows_per_proc + ((uint32_t)ctx->rank < remainder_rows ? 1 : 0);
	log_debug("comm_data start row %d rows per proc %d", comm_data->my_start_row, rows_per_proc);
    mpi_verify_distribution_range(comm_data);
}

int mpi_setup_scatter_gather_row_arrays(const struct mpi_context *ctx, const struct img_comm_data *comm_data, struct mpi_comm_arr *comm_arrays) {
    int proc_send_start = 0;
    uint32_t proc_send_end = 0;
    uint32_t proc_send_rows = 0;
    int current_packed_offset = 0;

    comm_arrays->sendcounts = NULL;
    comm_arrays->displs = NULL;
    comm_arrays->recvcounts = NULL;
    comm_arrays->recvdispls = NULL;

    if (ctx->rank != 0) {
        return 0;
    }

    comm_arrays->sendcounts = (int *)malloc((size_t)ctx->size * sizeof(int));
    comm_arrays->displs = (int *)malloc((size_t)ctx->size * sizeof(int));
    comm_arrays->recvcounts = (int *)malloc((size_t)ctx->size * sizeof(int));
    comm_arrays->recvdispls = (int *)malloc((size_t)ctx->size * sizeof(int));

    if (!comm_arrays->sendcounts || !comm_arrays->displs || !comm_arrays->recvcounts || !comm_arrays->recvdispls) {
        log_error("Rank 0: Failed to allocate memory for scatter/gather arrays.");
        free(comm_arrays->sendcounts);
        free(comm_arrays->displs);
        free(comm_arrays->recvcounts);
        free(comm_arrays->recvdispls);
        comm_arrays->sendcounts = comm_arrays->displs = comm_arrays->recvcounts = comm_arrays->recvdispls = NULL;
        return -1;
    }

    for (int i = 0; i < ctx->size; ++i) {
        struct img_comm_data temp_comm_data = {0};
        struct mpi_context temp_ctx = {i, ctx->size};

		temp_comm_data.dim = comm_data->dim;
        mpi_calculate_row_distribution(&temp_ctx, &temp_comm_data); // Calculate for rank i

        proc_send_start = (int)temp_comm_data.my_start_row - MPI_HALO_SIZE;
        proc_send_end = temp_comm_data.my_start_row + temp_comm_data.my_num_rows + MPI_HALO_SIZE;

        if (proc_send_start < 0) proc_send_start = 0;
        if (proc_send_end > comm_data->dim->height) proc_send_end = comm_data->dim->height;

        proc_send_rows = ( (uint32_t)proc_send_start < proc_send_end ) ? (proc_send_end - (uint32_t)proc_send_start) : 0;

        comm_arrays->sendcounts[i] = (int)(proc_send_rows * comm_data->row_stride_bytes);
        // Store original offset temporarily before adjusting for packing
        comm_arrays->displs[i] = (int)((uint32_t)proc_send_start * comm_data->row_stride_bytes);

        comm_arrays->recvcounts[i] = (int)(temp_comm_data.my_num_rows * comm_data->row_stride_bytes);
        comm_arrays->recvdispls[i] = (int)(temp_comm_data.my_start_row * comm_data->row_stride_bytes);
    }

    current_packed_offset = 0;
    for (int i = 0; i < ctx->size; ++i) {
        int displacement_in_packed_buffer = current_packed_offset;
        current_packed_offset += comm_arrays->sendcounts[i];
        comm_arrays->displs[i] = displacement_in_packed_buffer; // Overwrite with packed displacement
    }

    return 0;
}
