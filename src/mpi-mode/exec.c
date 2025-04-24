// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../libbmp/libbmp.h"
#include "../../logger/log.h"
#include "../utils/threads-general.h"
#include "../mt-mode/mt-types.h"
#include "filter-comp.h"
#include "rank0-proc.h"
#include "data-transfer.h"
#include "phases.h"
#include "utils.h"
#include "exec.h"
#include "mpi-types.h"
#include "utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Orchestrates image processing using row-based domain decomposition.
 * Manages the lifecycle of MPI communication and computation phases:
 * initialization, metadata broadcast, distribution calculation, communication setup,
 * data scattering, local processing, data gathering, and finalization.
 *
 * @param rank The rank of the current MPI process.
 * @param size The total number of MPI processes.
 * @param args Pointer to the structure containing parsed command-line arguments.
 * @param filters Pointer to the structure containing pre-initialized filter kernels.
 *
 * @return The total computation time in seconds, or a negative value on error.
 */
static double mpi_process_by_rows(int rank, int size, const struct p_args *args, const struct filter_mix *filters)
{
	struct mpi_context ctx = { rank, size };
	struct img_spec img_data = { 0 };
	struct img_comm_data comm_data = { 0 };
	struct mpi_comm_arr comm_arrays = { 0 };
	struct mpi_local_data local_data = { 0 };
	unsigned char *global_send_buffer = NULL;
	double start_time = 0.0;
	double final_time = -1.0;
	int8_t status = 0;

	if (!ctx.rank)
		log_info("Starting MPI process in BY_ROW mode");

	img_data.input_img = (bmp_img *)calloc(1, sizeof(bmp_img));
	img_data.output_img = (bmp_img *)calloc(1, sizeof(bmp_img));
	comm_data.dim = (struct img_dim *)malloc(sizeof(struct img_dim));

	if (!img_data.input_img || !img_data.output_img || !comm_data.dim) {
		log_error("Rank %d: Failed to allocate top-level structs.", rank);
		free(img_data.input_img);
		free(img_data.output_img);
		free(comm_data.dim);
		ABORT_AND_RETURN(-1.0);
	}

	comm_data.halo_size = get_halo_size(args->filter_type, filters);
	status = mpi_phase_initialize(&ctx, args, &img_data, &comm_data, &start_time);
	if (status != 0) {
		if (ctx.rank == 0)
			bmp_free_img_spec(&img_data); // Free bmp data if init failed
		free(img_data.input_img);
		free(img_data.output_img);
		free(comm_data.dim);
		ABORT_AND_RETURN(-1.0);
	}

	mpi_broadcast_metadata(&comm_data); // Broadcasts dims from rank 0
	if (ctx.rank != 0) {
		comm_data.row_stride_bytes = (size_t)comm_data.dim->width * BYTES_PER_PIXEL;
	}

	mpi_calculate_row_distribution(&ctx, &comm_data);

	status = mpi_phase_prepare_comm(&ctx, &comm_data, &img_data, &comm_arrays, &global_send_buffer);
	if (status != 0) {
		if (ctx.rank == 0) {
			free_comm_arr(comm_arrays);
			bmp_free_img_spec(&img_data);
		}
		free(img_data.input_img);
		free(img_data.output_img);
		free(comm_data.dim);
		ABORT_AND_RETURN(-1.0);
	}

	status = mpi_phase_scatter_data(&ctx, &comm_data, &local_data, global_send_buffer, &comm_arrays);
	if (status != 0) {
		mpi_phase_cleanup_resources(&ctx, &local_data, &comm_arrays);
		if (ctx.rank == 0)
			bmp_free_img_spec(&img_data);
		free(img_data.input_img);
		free(img_data.output_img);
		free(comm_data.dim);
		ABORT_AND_RETURN(-1.0);
	}

	mpi_compute_local_region(&local_data, &comm_data, args, filters, &ctx);

	status = mpi_phase_gather_data(&ctx, &comm_data, &local_data, &comm_arrays, &img_data);
	mpi_phase_cleanup_resources(&ctx, &local_data, &comm_arrays);
	if (status != 0) {
		if (ctx.rank == 0)
			bmp_free_img_spec(&img_data);
		free(img_data.input_img);
		free(img_data.output_img);
		free(comm_data.dim);
		ABORT_AND_RETURN(-1.0);
	}

	final_time = mpi_phase_finalize_and_broadcast(&ctx, start_time, &img_data, args);

	free(img_data.input_img);
	free(img_data.output_img);
	free(comm_data.dim);

	return final_time;
}

/** 
 * Orchestrates image processing using column-based domain decomposition via matrix transpose.
 * Manages the lifecycle of MPI communication and computation phases, including:
 * - Initial image read and timing start (rank 0).
 * - Transposing the input image pixel data (rank 0).
 * - Updating and broadcasting image metadata (dimensions, stride) to reflect the transpose.
 * - Calculating distribution and preparing communication based on transposed dimensions.
 * - Scattering the *transposed* image rows.
 * - Performing local processing using a filter implementation adapted for transposed data.
 * - Gathering the processed (transposed) rows.
 * - Transposing the final result back to the orig orientation (rank 0).
 * - Finalizing timing, saving the result, and cleaning up resources.
 *
 * @param rank The rank of the current MPI process.
 * @param size The total number of MPI processes.
 * @param args Pointer to the structure containing parsed command-line arguments.
 * @param filters Pointer to the structure containing pre-initialized filter kernels.
 *
 * @return The total computation time in seconds, or a negative value on error.
 *
 * p.s. rewriting it to <= 80 lines would be a spaghetti brainfuck. definitely not today, not tomorrow. best regards to @vkutuev
 */
static double mpi_process_by_columns(int rank, int size, const struct p_args *args, const struct filter_mix *filters)
{
	struct mpi_context ctx = { rank, size };
	struct img_spec img_data = { 0 };
	struct img_comm_data comm_data = { 0 };
	struct mpi_comm_arr comm_arrays = { 0 };
	struct mpi_local_data local_data = { 0 };
	unsigned char *global_send_buffer = NULL;
	bmp_pixel **orig_input_pixels = NULL;
	bmp_pixel **transposed_input_pixels = NULL;
	double start_time = 0.0;
	double final_time = -1.0;
	int8_t status = 0;
	uint32_t orig_width = 0, orig_height = 0;

	if (!ctx.rank)
		log_info("Starting MPI process in BY_COLUMN (Transposed) mode");

	img_data.input_img = (bmp_img *)calloc(1, sizeof(bmp_img));
	img_data.output_img = (bmp_img *)calloc(1, sizeof(bmp_img));
	comm_data.dim = (struct img_dim *)malloc(sizeof(struct img_dim));
	if (!img_data.input_img || !img_data.output_img || !comm_data.dim) {
		log_error("Rank %d: Failed to allocate top-level structs.", rank);
		goto ext_err_f;
	}

	comm_data.halo_size = get_halo_size(args->filter_type, filters);
	status = mpi_phase_initialize(&ctx, args, &img_data, &comm_data, &start_time);
	if (status) {
		if (!ctx.rank)
			bmp_free_img_spec(&img_data);
		goto ext_err_f;
	}

	orig_width = comm_data.dim->width; // saved, bc orig dim will be changed after transpose
	orig_height = comm_data.dim->height;

	if (ctx.rank == 0)
		mpi_rank0_transpose_img(&comm_data, &img_data, orig_width, orig_height);

	mpi_broadcast_metadata(&comm_data);
	mpi_calculate_column_distribution(&ctx, &comm_data);

	status = mpi_phase_prepare_comm(&ctx, &comm_data, &img_data, &comm_arrays, &global_send_buffer);
	if (status != 0) {
		if (ctx.rank == 0) {
			free_comm_arr(comm_arrays);
			if (transposed_input_pixels)
				bmp_img_pixel_free(transposed_input_pixels, comm_data.dim); // free only pixel array
			img_data.input_img->img_pixels = orig_input_pixels;
			bmp_free_img_spec(&img_data);
		}
		goto ext_err_f;
	}

	status = mpi_phase_scatter_data(&ctx, &comm_data, &local_data, global_send_buffer, &comm_arrays);
	if (status != 0) {
		mpi_phase_cleanup_resources(&ctx, &local_data, &comm_arrays);
		if (ctx.rank == 0) {
			if (transposed_input_pixels)
				bmp_img_pixel_free(transposed_input_pixels, comm_data.dim);
			img_data.input_img->img_pixels = orig_input_pixels;
			bmp_free_img_spec(&img_data);
		}
		goto ext_err_f;
	}

	// Rank 0: Free intermediate transposed input buffer now data is sent
	if (ctx.rank == 0 && transposed_input_pixels != NULL) {
		log_debug("Rank 0: Freeing intermediate transposed input pixel array post-scatter.");
		// bmp_img_pixel_free(transposed_input_pixels, comm_data.dim); // orig_width = transposed height TODO: fix
		img_data.input_img->img_pixels = orig_input_pixels; // Restore orig ptr for final free
		transposed_input_pixels = NULL;
	}

	mpi_compute_local_region(&local_data, &comm_data, args, filters, &ctx);
	log_trace("Finished computing local data");

	if (ctx.rank == 0)
		mpi_rank0_reinit_buffer_for_gather(&comm_data, &img_data, orig_width, orig_height);

	status = mpi_phase_gather_data(&ctx, &comm_data, &local_data, &comm_arrays, &img_data);
	mpi_phase_cleanup_resources(&ctx, &local_data, &comm_arrays);
	if (status != 0) {
		if (ctx.rank == 0)
			bmp_free_img_spec(&img_data); // Frees orig input and potentially gathered output
		goto ext_err_f;
	}

	if (ctx.rank == 0) {
		mpi_rank0_transpose_img_back(&comm_data, &img_data, orig_width, orig_height);
	}

	final_time = mpi_phase_finalize_and_broadcast(&ctx, start_time, &img_data, args);

	free(img_data.input_img);
	free(img_data.output_img);
	free(comm_data.dim);

	return final_time;

ext_err:
	bmp_free_img_spec(&img_data);

ext_err_f:
	free(img_data.input_img);
	free(img_data.output_img);
	free(comm_data.dim);
	ABORT_AND_RETURN(-1.0);
}

double execute_mpi_computation(uint8_t size, uint8_t rank, struct p_args *compute_args, struct filter_mix *filters)
{
	double total_time = 0;

	switch ((enum compute_mode)compute_args->compute_mode) {
	case BY_ROW:
		total_time = mpi_process_by_rows(rank, size, compute_args, filters);
		break;
	case BY_GRID:
		if (rank == 0)
			log_error("BY_GRID mode is not implemented for MPI.");
		MPI_Abort(MPI_COMM_WORLD, 1);
		return -1.0;
	case BY_PIXEL:
		if (rank == 0)
			log_error("BY_PIXEL mode is not practical for MPI static distribution.");
		MPI_Abort(MPI_COMM_WORLD, 1);
		return -1.0;
	case BY_COLUMN:
		total_time = mpi_process_by_columns(rank, size, compute_args, filters);
		break;
	default:
		if (rank == 0) {
			log_error("Error: Invalid compute_mode (%d) for MPI.", compute_args->compute_mode);
		}
		MPI_Abort(MPI_COMM_WORLD, 1);
		return -1.0;
	}
	return total_time;
}
