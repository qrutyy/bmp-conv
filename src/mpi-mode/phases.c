// SPDX-License-Identifier: GPL-3.0-or-later

#include "phases.h"
#include "../../libbmp/libbmp.h"
#include "../../logger/log.h"
#include "data-transfer.h"
#include "filter-comp.h"
#include "rank0-proc.h"
#include "utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For memcpy

int8_t mpi_phase_initialize(const struct mpi_context *ctx, const struct p_args *args, struct img_spec *img_data, struct img_comm_data *comm_data, double *start_time)
{
	int8_t setup_status = 0;

	if (!args || !args->input_filename[0]) {
		if (ctx->rank == 0)
			log_error("Rank 0: Error: Missing input filename for MPI mode.");
		return -1;
	}

	log_trace("INIT PHASE:\n");

	comm_data->dim = malloc(sizeof(struct img_dim));
	if (!comm_data) {
		log_error("Memory allocation failed");
		return -1;
	}
	comm_data->compute_mode = args->compute_mode;

	if (ctx->rank == 0) {
		setup_status = mpi_rank0_initialize(img_data, comm_data, start_time, args->input_filename[0]);
		if (setup_status != 0)

			return -1;
		comm_data->row_stride_bytes = (size_t)(comm_data->dim->width * BYTES_PER_PIXEL);
	}

	mpi_broadcast_metadata(comm_data); // Idk if all the processes should call it
	if (ctx->rank != 0) {
		comm_data->row_stride_bytes = (size_t)(comm_data->dim->width * BYTES_PER_PIXEL);
	}

	if (comm_data->dim->width <= 0 || comm_data->dim->height <= 0) {
		log_error("Rank %d: Received invalid image dimensions (%ux%u).", ctx->rank, comm_data->dim->width, comm_data->dim->height);
		return -1;
	}
	if (args->compute_mode == BY_ROW) { // Here we calculate distribution of image for every process.
		mpi_calculate_row_distribution(ctx, comm_data);
	} else {
		mpi_calculate_column_distribution(ctx, comm_data);
	}

	return 0;
}

int8_t mpi_phase_prepare_comm(const struct mpi_context *ctx, const struct img_comm_data *comm_data, const struct img_spec *img_data, struct mpi_comm_arr *comm_arrays,
			      unsigned char **global_send_buffer)
{
	int8_t setup_status = 0;
	int8_t pack_status = 0;
	int8_t i = 0;
	int16_t *displs_orig = NULL;

	*global_send_buffer = NULL;

	log_trace("PREPARE COMM PHASE:\n");

	if (ctx->rank == 0) {
		displs_orig = malloc((size_t)ctx->size * sizeof(int16_t));
		if (!displs_orig) {
			log_error("Rank 0: Failed to allocate memory for orig displacement array.");
			return -1;
		}
	}

	setup_status = mpi_setup_scatter_gather_row_arrays(ctx, comm_data, comm_arrays);
	if (setup_status != 0) {
		free(displs_orig);
		return -1;
	}

	if (setup_status == 0 && ctx->rank == 0) {
		for (i = 0; i < ctx->size; ++i) {
			log_debug("Rank 0: For rank %d: sendcount=%d, disp=%d, origdisp=%d, recvcount=%d, recvdisp=%d", i,
				  comm_arrays->sendcounts ? comm_arrays->sendcounts[i] : -1, comm_arrays->displs ? comm_arrays->displs[i] : -1,
				  comm_arrays->origdispls ? comm_arrays->origdispls[i] : -1, comm_arrays->recvcounts ? comm_arrays->recvcounts[i] : -1,
				  comm_arrays->recvdispls ? comm_arrays->recvdispls[i] : -1);
		}
		if (ctx->size > 1 && comm_arrays->sendcounts) {
			log_debug("Rank 0: Specifically, sendcounts[1] = %d (Rank 1 expects 4117230)", comm_arrays->sendcounts[1]);
		}
	}

	if (ctx->rank == 0) {
		pack_status = mpi_rank0_pack_data_for_scatter(img_data, comm_data, ctx, comm_arrays->sendcounts, comm_arrays->origdispls, global_send_buffer);
		if (pack_status != 0) {
			// Caller should free comm_arrays
			return -1;
		}
	}

	return 0;
}

void mpi_phase_process_region(const struct mpi_context *ctx, const struct img_comm_data *comm_data, const struct mpi_local_data *local_data, const struct p_args *args,
			      const struct filter_mix *filters)
{
	if (comm_data->my_num_rc > 0) {
		mpi_compute_local_region(local_data, comm_data, args, filters, ctx);
	} else {
		log_debug("Rank %d: Skipping local processing as my_num_rc is 0.", ctx->rank);
	}
}

double mpi_phase_finalize_and_broadcast(const struct mpi_context *ctx, double start_time, struct img_spec *img_data, const struct p_args *args)
{
	double total_time = -1.0;

	if (ctx->rank == 0) {
		total_time = mpi_rank0_finalize_and_save(ctx, start_time, img_data, args);
	}

	MPI_Bcast(&total_time, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

	if (!ctx->rank)
		log_debug("Rank %d: Final phase complete. Broadcast time: %.6f", ctx->rank, total_time);

	return total_time;
}
