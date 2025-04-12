// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../libbmp/libbmp.h"
#include "../../logger/log.h"
#include "row-compute.h"
#include "data-transfer.h"
#include "filter-comp.h"
#include "rank0-proc.h"
#include "utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For memcpy
#include "utils.h"

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
static int mpi_phase_initialize(const struct mpi_context *ctx, const struct p_args *args, struct img_spec *img_data, struct img_comm_data *comm_data, double *start_time)
{
	int setup_status = 0;

	if (!args || !args->input_filename[0]) {
		if (ctx->rank == 0)
			log_error("Rank 0: Error: Missing input filename for MPI mode.");
		return -1;
	}

	comm_data->dim = malloc(sizeof(struct img_dim));

	if (ctx->rank == 0) {
		setup_status = mpi_rank0_initialize(img_data, comm_data, start_time, args->input_filename[0]);
		if (setup_status != 0)
			return -1;
		comm_data->row_stride_bytes = (size_t)(comm_data->dim->width * BYTES_PER_PIXEL);
	}

	mpi_broadcast_metadata(comm_data); // idk if all the processes should call it
	if (ctx->rank != 0) {
		comm_data->row_stride_bytes = (size_t)(comm_data->dim->width * BYTES_PER_PIXEL);
	}

	if (comm_data->dim->width <= 0 || comm_data->dim->height <= 0) {
		log_error("Rank %d: Received invalid image dimensions (%ux%u).", ctx->rank, comm_data->dim->width, comm_data->dim->height);
		return -1;
	}

	mpi_calculate_row_distribution(ctx, comm_data);

	return 0;
}

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
static int mpi_phase_prepare_comm(const struct mpi_context *ctx, const struct img_comm_data *comm_data, const struct img_spec *img_data, struct mpi_comm_arr *comm_arrays,
				  unsigned char **global_send_buffer)
{
	int setup_status = 0;
	int pack_status = 0;
	int *displs_original = NULL;
	struct mpi_pack_params pack_params = { 0 };
	size_t i = 0;

	*global_send_buffer = NULL;

	if (ctx->rank == 0) {
		displs_original = (int *)malloc((size_t)ctx->size * sizeof(int));
		if (!displs_original) {
			log_error("Rank 0: Failed to allocate memory for original displacement array.");
			return -1;
		}
	}

	setup_status = mpi_setup_scatter_gather_row_arrays(ctx, comm_data, comm_arrays);
	if (setup_status != 0) {
		free(displs_original);
		return -1;
	}

	if (ctx->rank == 0) {
		for (i = 0; i < ctx->size; ++i) {
			struct img_comm_data temp_comm_data = { 0 }; // create temporary to avoid modifying original
			temp_comm_data.dim = comm_data->dim; // copy necessary fields
			struct mpi_context temp_ctx = { i, ctx->size };
			mpi_calculate_row_distribution(&temp_ctx, &temp_comm_data); // calculate for rank i
			int temp_send_start = (int)temp_comm_data.my_start_row - MPI_HALO_SIZE;
			if (temp_send_start < 0)
				temp_send_start = 0;
			displs_original[i] = (int)((uint32_t)temp_send_start * comm_data->row_stride_bytes);
			log_debug("Rank %ux: displs %d, my_start_row %d\n", i, displs_original[i], temp_comm_data.my_start_row);
		}

		pack_params.sendcounts = comm_arrays->sendcounts;
		pack_params.displs_original = displs_original;

		pack_status = mpi_rank0_pack_data_for_scatter(img_data, comm_data, ctx, &pack_params, global_send_buffer);
		free(displs_original);
		if (pack_status != 0) {
			// Caller should free comm_arrays
			return -1;
		}
	}

	return 0;
}

// just verifies the input and calls mpi_process_local_region
static void mpi_phase_process_region(const struct mpi_context *ctx, const struct img_comm_data *comm_data, const struct mpi_local_data *local_data, const struct p_args *args,
				     const struct filter_mix *filters)
{
	if (comm_data->my_num_rows > 0) {
		mpi_compute_local_region(local_data, comm_data, args->filter_type, filters);
	} else {
		log_debug("Rank %d: Skipping local processing as my_num_rows is 0.", ctx->rank);
	}
}

// see `mpi_rank0_finalize_and_save` doc
static double mpi_phase_finalize_and_broadcast(const struct mpi_context *ctx, double start_time, struct img_spec *img_data, const struct p_args *args)
{
	double total_time = -1.0;

	if (ctx->rank == 0) {
		total_time = mpi_rank0_finalize_and_save(ctx, start_time, img_data, args);
	}

	MPI_Bcast(&total_time, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

	log_debug("Rank %d: Final phase complete. Broadcast time: %.6f", ctx->rank, total_time);
	return total_time;
}

//doc'd in header
double mpi_process_by_rows(int rank, int size, const struct p_args *args, const struct filter_mix *filters)
{
	struct mpi_context ctx = { rank, size };
	log_debug("SIZE %d", size);
	struct img_spec img_data = { 0 };
	struct img_comm_data comm_data = { 0 };
	struct mpi_comm_arr comm_arrays = { 0 };
	struct mpi_local_data local_data = { 0 };
	unsigned char *global_send_buffer = NULL;
	double start_time = 0.0;
	double final_time = -1.0;
	int status = 0;

	img_data.input_img = (bmp_img *)calloc(1, sizeof(bmp_img));
	img_data.output_img = (bmp_img *)calloc(1, sizeof(bmp_img));

	if (!img_data.input_img || !img_data.output_img) {
		log_error("Rank %d: Failed to allocate top-level bmp_img structs.", rank);
		free_img_spec(&img_data);
		ABORT_AND_RETURN(-1.0);
	}

	status = mpi_phase_initialize(&ctx, args, &img_data, &comm_data, &start_time);
	if (status != 0) {
		free_img_spec(&img_data);
		ABORT_AND_RETURN(-1.0);
	}

	status = mpi_phase_prepare_comm(&ctx, &comm_data, &img_data, &comm_arrays, &global_send_buffer);
	if (status != 0) {
		if (ctx.rank == 0) {
			free_comm_arr(comm_arrays);
			bmp_free_img_spec(&img_data);
		}
		free_img_spec(&img_data);
		ABORT_AND_RETURN(-1.0);
	}

	status = mpi_phase_scatter_data(&ctx, &comm_data, &local_data, global_send_buffer, &comm_arrays);
	if (status != 0) {
		mpi_phase_cleanup_resources(&ctx, &local_data, &comm_arrays);
		if (ctx.rank == 0)
			bmp_free_img_spec(&img_data);
		free_img_spec(&img_data);
		ABORT_AND_RETURN(-1.0);
	}

	mpi_phase_process_region(&ctx, &comm_data, &local_data, args, filters);

	status = mpi_phase_gather_data(&ctx, &comm_data, &local_data, &comm_arrays, &img_data);
	if (status != 0) {
		mpi_phase_cleanup_resources(&ctx, &local_data, &comm_arrays);
		if (ctx.rank == 0)
			bmp_free_img_spec(&img_data);
		free_img_spec(&img_data);

		ABORT_AND_RETURN(-1.0);
	}

	mpi_phase_cleanup_resources(&ctx, &local_data, &comm_arrays);

	final_time = mpi_phase_finalize_and_broadcast(&ctx, start_time, &img_data, args);

	// Outer structs freed after use in finalize
	free(img_data.input_img);
	free(img_data.output_img);

	return final_time;
}
