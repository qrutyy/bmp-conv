// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../libbmp/libbmp.h"
#include "../../logger/log.h"
#include "exec.h"
#include "mpi-types.h"
#include "data-transfer.h"
#include "utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For memcpy
#include "utils.h"
#include "../mt-mode/compute.h"
#include "../utils/threads-general.h"
#include "filter-comp.h"
#include "phases.h"


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

	if (!ctx.rank) log_info("Starting MPI process in BY_ROW mode");

	img_data.input_img = (bmp_img *)calloc(1, sizeof(bmp_img));
	img_data.output_img = (bmp_img *)calloc(1, sizeof(bmp_img));
    comm_data.dim = (struct img_dim *)malloc(sizeof(struct img_dim));

	if (!img_data.input_img || !img_data.output_img || !comm_data.dim) {
		log_error("Rank %d: Failed to allocate top-level structs.", rank);
		free(img_data.input_img); free(img_data.output_img); free(comm_data.dim);
		ABORT_AND_RETURN(-1.0);
	}

	comm_data.halo_size = get_halo_size(args->filter_type, filters);
	status = mpi_phase_initialize(&ctx, args, &img_data, &comm_data, &start_time);
	if (status != 0) {
        if (ctx.rank == 0) bmp_free_img_spec(&img_data); // Free bmp data if init failed
		free(img_data.input_img); free(img_data.output_img); free(comm_data.dim);
		ABORT_AND_RETURN(-1.0);
	}
    // Original dimensions are now in comm_data.dim

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
		free(img_data.input_img); free(img_data.output_img); free(comm_data.dim);
		ABORT_AND_RETURN(-1.0);
	}

	status = mpi_phase_scatter_data(&ctx, &comm_data, &local_data, global_send_buffer, &comm_arrays);
	if (status != 0) {
		mpi_phase_cleanup_resources(&ctx, &local_data, &comm_arrays);
        if (ctx.rank == 0) bmp_free_img_spec(&img_data);
		free(img_data.input_img); free(img_data.output_img); free(comm_data.dim);
		ABORT_AND_RETURN(-1.0);
	}

	mpi_compute_local_region(&local_data, &comm_data, args, filters);

	status = mpi_phase_gather_data(&ctx, &comm_data, &local_data, &comm_arrays, &img_data);
    mpi_phase_cleanup_resources(&ctx, &local_data, &comm_arrays);
	if (status != 0) {
        if (ctx.rank == 0) bmp_free_img_spec(&img_data);
		free(img_data.input_img); free(img_data.output_img); free(comm_data.dim);
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
 * - Transposing the final result back to the original orientation (rank 0).
 * - Finalizing timing, saving the result, and cleaning up resources.
 *
 * @param rank The rank of the current MPI process.
 * @param size The total number of MPI processes.
 * @param args Pointer to the structure containing parsed command-line arguments.
 * @param filters Pointer to the structure containing pre-initialized filter kernels.
 *
 * @return The total computation time in seconds, or a negative value on error.
 */
static double mpi_process_by_columns(int rank, int size, const struct p_args *args, const struct filter_mix *filters)
{
	struct mpi_context ctx = { rank, size };
	struct img_spec img_data = { 0 };
	struct img_comm_data comm_data = { 0 };
	struct mpi_comm_arr comm_arrays = { 0 };
	struct mpi_local_data local_data = { 0 };
	unsigned char *global_send_buffer = NULL;
    bmp_pixel **original_input_pixels = NULL;
    bmp_pixel **transposed_input_pixels = NULL;
	double start_time = 0.0;
	double final_time = -1.0;
	int8_t status = 0;
    uint32_t original_width = 0, original_height = 0;

	if (!ctx.rank) log_info("Starting MPI process in BY_COLUMN (Transposed) mode");

	img_data.input_img = (bmp_img *)calloc(1, sizeof(bmp_img));
	img_data.output_img = (bmp_img *)calloc(1, sizeof(bmp_img));
    comm_data.dim = (struct img_dim *)malloc(sizeof(struct img_dim));

	if (!img_data.input_img || !img_data.output_img || !comm_data.dim) {
		log_error("Rank %d: Failed to allocate top-level structs.", rank);
        free(img_data.input_img); free(img_data.output_img); free(comm_data.dim);
		ABORT_AND_RETURN(-1.0);
	}

	comm_data.halo_size = get_halo_size(args->filter_type, filters);
	status = mpi_phase_initialize(&ctx, args, &img_data, &comm_data, &start_time);
	if (status != 0) {
        if (ctx.rank == 0) bmp_free_img_spec(&img_data);
        free(img_data.input_img); free(img_data.output_img); free(comm_data.dim);
		ABORT_AND_RETURN(-1.0);
	}
    original_width = comm_data.dim->width;
    original_height = comm_data.dim->height;

    if (ctx.rank == 0) {
        log_info("Rank 0: Transposing input image...");
        original_input_pixels = img_data.input_img->img_pixels; // Keep original
        transposed_input_pixels = transpose_matrix(original_input_pixels, comm_data.dim);
        if (!transposed_input_pixels) {
            log_error("Rank 0: Failed to transpose input matrix.");
            bmp_free_img_spec(&img_data); // Frees original pixels
            free(img_data.input_img); free(img_data.output_img); free(comm_data.dim);
            ABORT_AND_RETURN(-1.0);
        }
        img_data.input_img->img_pixels = transposed_input_pixels; // Use transposed

        comm_data.dim->width = original_height;
        comm_data.dim->height = original_width;
        comm_data.row_stride_bytes = (size_t)comm_data.dim->width * BYTES_PER_PIXEL;
        log_info("Rank 0: Swapped dimensions for transpose: %ux%u, new stride: %zu",
                 comm_data.dim->height, comm_data.dim->width, comm_data.row_stride_bytes);

        img_data.input_img->img_header.biWidth = comm_data.dim->width;
        img_data.input_img->img_header.biHeight = comm_data.dim->height;
    }

	mpi_broadcast_metadata(&comm_data);
	if (ctx.rank != 0) {
        comm_data.row_stride_bytes = (size_t)comm_data.dim->width * BYTES_PER_PIXEL;
	}
	mpi_calculate_row_distribution(&ctx, &comm_data);

	status = mpi_phase_prepare_comm(&ctx, &comm_data, &img_data, &comm_arrays, &global_send_buffer);
	if (status != 0) {
		if (ctx.rank == 0) {
            free_comm_arr(comm_arrays);
            if (transposed_input_pixels) bmp_img_pixel_free(transposed_input_pixels, comm_data.dim);
            img_data.input_img->img_pixels = original_input_pixels;
            bmp_free_img_spec(&img_data); // Frees original pixels now
        }
        free(img_data.input_img); free(img_data.output_img); free(comm_data.dim);
		ABORT_AND_RETURN(-1.0);
	}

	status = mpi_phase_scatter_data(&ctx, &comm_data, &local_data, global_send_buffer, &comm_arrays);
	if (status != 0) {
		mpi_phase_cleanup_resources(&ctx, &local_data, &comm_arrays);
        if (ctx.rank == 0) {
            if (transposed_input_pixels) bmp_img_pixel_free(transposed_input_pixels, comm_data.dim);
            img_data.input_img->img_pixels = original_input_pixels;
            bmp_free_img_spec(&img_data);
        }
        free(img_data.input_img); free(img_data.output_img); free(comm_data.dim);
		ABORT_AND_RETURN(-1.0);
	}
    // Rank 0: Free intermediate transposed input buffer now data is sent
    if (ctx.rank == 0 && transposed_input_pixels != NULL) {
        log_debug("Rank 0: Freeing intermediate transposed input pixel array post-scatter.");
        bmp_img_pixel_free(transposed_input_pixels, comm_data.dim); // original_width = transposed height
        img_data.input_img->img_pixels = original_input_pixels; // Restore original ptr for final free
        transposed_input_pixels = NULL;
    }

	mpi_compute_local_region(&local_data, &comm_data, args, filters);

    if (ctx.rank == 0) {
        img_data.output_img->img_header.biWidth = comm_data.dim->width;
        img_data.output_img->img_header.biHeight = comm_data.dim->height;
        log_debug("Rank 0: Adjusted output header for gather: %dx%d",
                  img_data.output_img->img_header.biWidth, img_data.output_img->img_header.biHeight);
    }
	status = mpi_phase_gather_data(&ctx, &comm_data, &local_data, &comm_arrays, &img_data);
    mpi_phase_cleanup_resources(&ctx, &local_data, &comm_arrays);
	if (status != 0) {
        if (ctx.rank == 0) bmp_free_img_spec(&img_data); // Frees original input and potentially gathered output
        free(img_data.input_img); free(img_data.output_img); free(comm_data.dim);
		ABORT_AND_RETURN(-1.0);
	}

    if (ctx.rank == 0) {
        log_info("Rank 0: Transposing output image back...");
        bmp_pixel **gathered_transposed_pixels = img_data.output_img->img_pixels;

        bmp_pixel **final_output_pixels = transpose_matrix(gathered_transposed_pixels,
                                                                  comm_data.dim);
        if (!final_output_pixels) {
            log_error("Rank 0: Failed to transpose output matrix back.");
            bmp_img_pixel_free(gathered_transposed_pixels, comm_data.dim); 
			bmp_free_img_spec(&img_data); // Free original input
            free(img_data.input_img); free(img_data.output_img); free(comm_data.dim);
            ABORT_AND_RETURN(-1.0);
        }

//        bmp_img_pixel_free(gathered_transposed_pixels, comm_data.dim); 
        img_data.output_img->img_pixels = final_output_pixels; 

        img_data.output_img->img_header.biWidth = original_width;
        img_data.output_img->img_header.biHeight = original_height;
        log_info("Rank 0: Restored output dimensions: %dx%d",
                 img_data.output_img->img_header.biWidth, img_data.output_img->img_header.biHeight);
    }

	final_time = mpi_phase_finalize_and_broadcast(&ctx, start_time, &img_data, args);

	free(img_data.input_img);
	free(img_data.output_img);
    free(comm_data.dim);

	return final_time;
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

