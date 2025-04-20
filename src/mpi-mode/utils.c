// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../logger/log.h"
#include "../../libbmp/libbmp.h"
#include "../utils/threads-general.h"
#include "mpi-types.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

uint8_t get_halo_size(const char *filter_type, const struct filter_mix *filters)
{
	int filter_size = 0, halo_size = 0;

	if (!filter_type || !filters) {
		log_error("get_halo_size: Invalid NULL arguments.");
		return 0;
	}

	// Determine the size of the selected filter kernel
	if (strcmp(filter_type, "mb") == 0 && filters->motion_blur) {
		filter_size = filters->motion_blur->size;
	} else if (strcmp(filter_type, "bb") == 0 && filters->blur) {
		filter_size = filters->blur->size;
	} else if (strcmp(filter_type, "gb") == 0 && filters->gaus_blur) {
		filter_size = filters->gaus_blur->size;
	} else if (strcmp(filter_type, "co") == 0 && filters->conv) {
		filter_size = filters->conv->size;
	} else if (strcmp(filter_type, "sh") == 0 && filters->sharpen) {
		filter_size = filters->sharpen->size;
	} else if (strcmp(filter_type, "em") == 0 && filters->emboss) {
		filter_size = filters->emboss->size;
	} else if (strcmp(filter_type, "mm") == 0) {
		filter_size = 15; // fixed at this moment
	} else if (strcmp(filter_type, "gg") == 0 && filters->big_gaus) {
		filter_size = filters->big_gaus->size;
	} else if (strcmp(filter_type, "bo") == 0 && filters->box_blur) {
		filter_size = filters->box_blur->size;
	} else if (strcmp(filter_type, "mg") == 0 && filters->med_gaus) {
		filter_size = filters->med_gaus->size;
	} else {
		log_warn("get_halo_size: Unknown or unsupported filter type '%s'. Returning halo size 0.", filter_type);
		return 0;
	}

	if (filter_size <= 0) {
		log_warn("get_halo_size: Filter '%s' has invalid size %d. Returning halo size 0.", filter_type, filter_size);
		return 0;
	}

	halo_size = (uint8_t)(filter_size / 2);
	return halo_size;
}

void free_comm_arr(struct mpi_comm_arr comm_arrays)
{
	free(comm_arrays.sendcounts);
	free(comm_arrays.displs);
	free(comm_arrays.recvcounts);
	free(comm_arrays.recvdispls);
}

void mpi_phase_cleanup_resources(const struct mpi_context *ctx, struct mpi_local_data *local_data, struct mpi_comm_arr *comm_arrays)
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

int8_t mpi_allocate_local_buffers(const struct mpi_context *ctx, const struct img_comm_data *comm_data, struct mpi_local_data *local_data)
{
	size_t input_buf_size = 0;
	size_t output_buf_size = 0;

	input_buf_size = (size_t)comm_data->send_num_rows * comm_data->row_stride_bytes;
	output_buf_size = (size_t)comm_data->my_num_rows * comm_data->row_stride_bytes;

	log_debug("Rank %d: Attempting to allocate input: %zu bytes, output: %zu bytes", ctx->rank, input_buf_size, output_buf_size);

	local_data->input_pixels = (unsigned char *)malloc(input_buf_size > 0 ? input_buf_size : 1);
	local_data->output_pixels = (unsigned char *)malloc(output_buf_size > 0 ? output_buf_size : 1);

	if (!local_data->input_pixels || !local_data->output_pixels) {
		log_error("Rank %d: Failed to allocate local pixel buffers (in: %zu B, out: %zu B).", ctx->rank, input_buf_size, output_buf_size);
		free(local_data->input_pixels);
		free(local_data->output_pixels);
		local_data->input_pixels = local_data->output_pixels = NULL;
		return -1;
	}

	return 0;
}

void mpi_verify_distribution_range(struct img_comm_data *comm_data)
{
	int start_row_with_halo = (int)comm_data->my_start_row - comm_data->halo_size;
	uint32_t end_row_with_halo = comm_data->my_start_row + comm_data->my_num_rows + comm_data->halo_size;

	if (start_row_with_halo < 0) {
		comm_data->send_start_row = 0;
	} else {
		comm_data->send_start_row = (uint32_t)start_row_with_halo;
	}

	if (end_row_with_halo > comm_data->dim->height) {
		end_row_with_halo = comm_data->dim->height;
	}

	if (comm_data->send_start_row >= end_row_with_halo) {
		comm_data->send_num_rows = 0;
	} else {
		comm_data->send_num_rows = end_row_with_halo - comm_data->send_start_row;
	}
	log_trace("Rank %u: Verified distribution: send_start=%u, send_end=%u, send_num_rows=%u (my_start=%u, my_num=%u)", comm_data->my_start_row, comm_data->send_start_row,
		  end_row_with_halo, comm_data->send_num_rows, comm_data->my_start_row, comm_data->my_num_rows);
}

void mpi_calculate_row_distribution(const struct mpi_context *ctx, struct img_comm_data *comm_data)
{
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
	log_debug("comm_data start row %d rows per proc %d, i = %d", comm_data->my_start_row, rows_per_proc, ctx->rank);
	log_debug("end row %d", comm_data->my_start_row + comm_data->my_num_rows);
	mpi_verify_distribution_range(comm_data);
}

void mpi_calculate_column_distribution(const struct mpi_context *ctx, struct img_comm_data *comm_data)
{
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
	log_debug("comm_data start row %d rows per proc %d, i = %d", comm_data->my_start_row, rows_per_proc, ctx->rank);
	log_debug("end row %d", comm_data->my_start_row + comm_data->my_num_rows);
	mpi_verify_distribution_range(comm_data);
}
int8_t mpi_setup_scatter_gather_row_arrays(const struct mpi_context *ctx, const struct img_comm_data *comm_data, struct mpi_comm_arr *comm_arrays)
{
	int proc_send_start = 0;
	uint32_t proc_send_end = 0;
	uint32_t proc_send_rows = 0;
	int current_packed_offset = 0;
	size_t i = 0;

	comm_arrays->sendcounts = NULL;
	comm_arrays->displs = NULL;
	comm_arrays->recvcounts = NULL;
	comm_arrays->recvdispls = NULL;

	if (ctx->rank != 0) {
		return 0;
	}

	comm_arrays->sendcounts = malloc((size_t)ctx->size * sizeof(int));
	comm_arrays->displs = malloc((size_t)ctx->size * sizeof(int));
	comm_arrays->recvcounts = malloc((size_t)ctx->size * sizeof(int));
	comm_arrays->recvdispls = malloc((size_t)ctx->size * sizeof(int));
	comm_arrays->origdispls = malloc((size_t)ctx->size * sizeof(int));

	if (!comm_arrays->sendcounts || !comm_arrays->displs || !comm_arrays->recvcounts || !comm_arrays->recvdispls) {
		log_error("Rank 0: Failed to allocate memory for scatter/gather arrays.");
		free(comm_arrays->sendcounts);
		free(comm_arrays->displs);
		free(comm_arrays->recvcounts);
		free(comm_arrays->recvdispls);
		comm_arrays->sendcounts = comm_arrays->displs = comm_arrays->recvcounts = comm_arrays->recvdispls = NULL;
		return -1;
	}

	for (i = 0; i < ctx->size; ++i) {
		struct img_comm_data temp_comm_data = { 0 };
		struct mpi_context temp_ctx = { i, ctx->size };

		temp_comm_data.dim = comm_data->dim;
		temp_comm_data.halo_size = comm_data->halo_size;
		log_debug("Calling calc_row_distr with rank = %d", temp_ctx.rank);
		// For configuring the scatterv and gatterv operations (etc. array setting) - we need to calculate it another time for each process but only from the root perspective.
		mpi_calculate_row_distribution(&temp_ctx, &temp_comm_data); // Calculate for rank i

		proc_send_start = (int)temp_comm_data.my_start_row - comm_data->halo_size;
		proc_send_end = temp_comm_data.my_start_row + temp_comm_data.my_num_rows + comm_data->halo_size;

		if (proc_send_start < 0)
			proc_send_start = 0;
		if (proc_send_end > comm_data->dim->height)
			proc_send_end = comm_data->dim->height;

		proc_send_rows = proc_send_end - (uint32_t)proc_send_start;

		comm_arrays->sendcounts[i] = (int)(proc_send_rows * comm_data->row_stride_bytes);
		comm_arrays->origdispls[i] = (int)((uint32_t)proc_send_start * comm_data->row_stride_bytes);

		comm_arrays->recvcounts[i] = (int)(temp_comm_data.my_num_rows * comm_data->row_stride_bytes);
		comm_arrays->recvdispls[i] = (int)(temp_comm_data.my_start_row * comm_data->row_stride_bytes);
	}

	current_packed_offset = 0;
	for (i = 0; i < ctx->size; ++i) {
		comm_arrays->displs[i] = current_packed_offset;
		current_packed_offset += comm_arrays->sendcounts[i];
	}

	return 0;
}
