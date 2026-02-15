// SPDX-License-Identifier: GPL-3.0-or-later

#include "rank0-proc.h"
#include "logger/log.h"
#include "libbmp/libbmp.h"
#include "utils/threads-general.h"
#include "../utils/mpi-types.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mpi.h>

int8_t mpi_rank0_initialize(struct img_spec *img_data, struct img_comm_data *comm_data, double *start_time, const char *input_filename_base)
{
	char input_filepath[256] = { 0 };
	int8_t read_status = -1;

	log_info("Rank 0: Initializing MPI computation...");
	snprintf(input_filepath, sizeof(input_filepath), "test-img/%s", input_filename_base);

	read_status = bmp_img_read(img_data->input, input_filepath);
	if (read_status != 0) {
		log_error("Rank 0: Error: Could not read BMP image '%s'.", input_filepath);
		return -1;
	}

	comm_data->dim->width = img_data->input->img_header.biWidth;
	comm_data->dim->height = img_data->input->img_header.biHeight;

	if (comm_data->dim->height < 1 || comm_data->dim->width < 1) {
		log_error("Rank 0: Error: Invalid image dimensions (%ux%u).", comm_data->dim->width, comm_data->dim->height);
		bmp_img_free(img_data->input);
		return -1;
	}
	bmp_print_header_data(&img_data->input->img_header);

	bmp_img_init_df(img_data->output, comm_data->dim->width, comm_data->dim->height);
	img_data->output->img_header = img_data->input->img_header;

	*start_time = MPI_Wtime();
	log_info("Rank 0: Image '%s' (%ux%u) read successfully.", input_filepath, comm_data->dim->height, comm_data->dim->width);
	return 0;
}

double mpi_rank0_finalize_and_save(const struct mpi_context *ctx, double start_time, struct img_spec *img_data, const struct p_args *args)
{
	double end_time = 0.0;
	double total_time = -1.0;
	char output_filepath[256] = { 0 };

	if (ctx->rank == 0) {
		end_time = MPI_Wtime();
		total_time = end_time - start_time;

		log_info("Rank 0: MPI computation finished in %.6f seconds.", total_time);

		if (args) {
			save_result_image(output_filepath, sizeof(output_filepath), -1, img_data->output, args);
			log_info("Rank 0: MPI result saved to %s", output_filepath);
		} else {
			log_warn("Rank 0: Cannot save result, args not provided.");
		}

		bmp_print_header_data(&img_data->output->img_header);

		bmp_print_header_data(&img_data->input->img_header); // note that dims are swapped!
		//		bmp_compare_images(img_data->input, img_data->output);

		bmp_img_free(img_data->output);
		bmp_img_free(img_data->input);
	}

	return total_time;
}

int8_t mpi_rank0_pack_data_for_scatter(const struct img_spec *img_data, const struct img_comm_data *comm_data, const struct mpi_context *ctx, const int *sendcounts,
				       const int *displs_original, unsigned char **packed_buffer)
{
	size_t total_packed_size = 0;
	unsigned char *current_pack_ptr = NULL;
	uint32_t proc_start_row = 0;
	uint32_t proc_send_rows = 0;
	size_t i = 0;
	uint32_t r = 0;

	*packed_buffer = NULL;

	for (i = 0; i < ctx->size; ++i) {
		total_packed_size += (size_t)sendcounts[i];
	}

	if (total_packed_size == 0) {
		log_warn("Rank 0: Total packed size is 0, nothing to pack.");
		return 0;
	}

	*packed_buffer = (unsigned char *)malloc(total_packed_size);
	if (!*packed_buffer) {
		log_error("Rank 0: Failed to allocate packed buffer for scattering (%zu bytes).", total_packed_size);
		return -1;
	}

	current_pack_ptr = *packed_buffer;
	// iterates through processes and packs the image data (see header)
	for (i = 0; i < ctx->size; ++i) {
		if (sendcounts[i] > 0) {
			proc_start_row = (uint32_t)(displs_original[i] / comm_data->row_stride_bytes);
			proc_send_rows = (uint32_t)(sendcounts[i] / comm_data->row_stride_bytes);

			if (proc_start_row + proc_send_rows > comm_data->dim->height) {
				log_error("Rank 0: Packing error - calculated rows exceed image height for rank %d. Height %d and end_row %lu", i, comm_data->dim->height,
					  proc_start_row + proc_send_rows);
				log_error("Rank 0: start %lu, count %lu", proc_start_row, proc_send_rows);

				free(*packed_buffer);
				*packed_buffer = NULL;
				return -1;
			}

			// copies proc_send_rows amount of rows into the packed_buffer
			for (r = 0; r < proc_send_rows; ++r) {
				if (img_data->input->img_pixels && img_data->input->img_pixels[proc_start_row + r]) {
					memcpy(current_pack_ptr, img_data->input->img_pixels[proc_start_row + r], comm_data->row_stride_bytes);
					current_pack_ptr += comm_data->row_stride_bytes;
				} else {
					log_error("Rank 0: Packing error - source row pointer is NULL for row %u.", proc_start_row + r);
					free(*packed_buffer);
					*packed_buffer = NULL;
					return -1;
				}
			}
		}
	}

	return 0;
}

int8_t mpi_rank0_unpack_data(const unsigned char *gathered_buffer, struct img_spec *img_data, const struct img_comm_data *comm_data, const struct mpi_context *ctx,
			     const struct mpi_comm_arr *comm_arrays)
{
	const unsigned char *current_unpack_ptr = NULL;
	uint32_t proc_start_row = 0;
	uint32_t proc_num_rows = 0;
	size_t i = 0;
	uint32_t r = 0;

	if (!gathered_buffer || !img_data || !img_data->output || !img_data->output->img_pixels) {
		log_error("Rank 0: Cannot unpack data, null buffer or result image structure provided.");
		return -1;
	}

	for (i = 0; i < ctx->size; ++i) {
		if (comm_arrays->recvcounts[i] > 0) {
			proc_start_row = (uint32_t)(comm_arrays->recvdispls[i] / comm_data->row_stride_bytes);
			proc_num_rows = (uint32_t)(comm_arrays->recvcounts[i] / comm_data->row_stride_bytes);

			if (proc_start_row + proc_num_rows > comm_data->dim->height) {
				log_error("Rank 0: Unpacking error - calculated rows exceed image height for rank %d.", i);
				return -1;
			}

			current_unpack_ptr = gathered_buffer + comm_arrays->recvdispls[i]; // shift position to displacement

			for (r = 0; r < proc_num_rows; ++r) {
				if (img_data->output->img_pixels[proc_start_row + r] == NULL) {
					log_error("Rank 0: Unpacking error - destination row %u is NULL.", proc_start_row + r);
					return -1;
				}
				memcpy(img_data->output->img_pixels[proc_start_row + r], current_unpack_ptr, comm_data->row_stride_bytes);
				current_unpack_ptr += comm_data->row_stride_bytes;
			}
		}
	}
	return 0;
}

int8_t mpi_rank0_reinit_buffer_for_gather(struct img_comm_data *comm_data, struct img_spec *img_data, uint32_t width, uint32_t height)
{
	log_debug("Rank 0: Re-initializing output image buffer for Gather phase with transposed dimensions H'=%u, W'=%u", comm_data->dim->height, comm_data->dim->width);
	if (img_data->output->img_pixels) {
		// Free the buffer allocated with original dimensions during initialize
		log_trace("Rank 0: Freeing output buffer allocated with original dimensions.");
		struct img_dim original_dim_temp = { .width = width, .height = height };
		bmp_img_pixel_free(img_data->output->img_pixels, &original_dim_temp);
		img_data->output->img_pixels = NULL;
	}
	// Allocate output pixel buffer with TRANSPOSED dimensions (H'=2001, W'=1226)
	img_data->output->img_pixels = bmp_img_pixel_alloc(comm_data->dim->height, comm_data->dim->width);
	if (!img_data->output->img_pixels) {
		log_error("Rank 0: Failed to allocate output buffer with transposed dimensions.");
		ABORT_AND_RETURN(-1.0);
	}
	// Update output header to match the buffer allocated for gather (transposed dims)
	img_data->output->img_header.biWidth = comm_data->dim->width; // W'=1226
	img_data->output->img_header.biHeight = comm_data->dim->height; // H'=2001

	return 0;
}

int8_t mpi_rank0_transpose_img(struct img_comm_data *comm_data, struct img_spec *img_data, uint32_t width, uint32_t height)
{
	log_info("Rank 0: Transposing image...");
	bmp_pixel **gathered_transposed_pixels = img_data->input->img_pixels;

	bmp_pixel **final_output_pixels = transpose_matrix(gathered_transposed_pixels, comm_data->dim);
	if (!final_output_pixels) {
		log_error("Rank 0: Failed to transpose output matrix back.");
		bmp_img_pixel_free(gathered_transposed_pixels, comm_data->dim);
		bmp_free_img_spec(img_data);
		free(img_data->input);
		free(img_data->output);
		free(comm_data->dim);
		ABORT_AND_RETURN(-1.0);
	}

	img_data->input->img_pixels = final_output_pixels;
	img_data->input->img_header.biWidth = comm_data->dim->width = height;
	img_data->input->img_header.biHeight = comm_data->dim->height = width;
	comm_data->row_stride_bytes = (size_t)comm_data->dim->width * BYTES_PER_PIXEL;
	log_info("Rank 0: Swapped dimensions for transpose: %ux%u (orig %ux%u), new stride: %zu", comm_data->dim->height, comm_data->dim->width, height, width,
		 comm_data->row_stride_bytes);

	return 0;
}

int8_t mpi_rank0_transpose_img_back(struct img_comm_data *comm_data, struct img_spec *img_data, uint32_t width, uint32_t height)
{
	log_info("Rank 0: Transposing image...");
	bmp_pixel **gathered_transposed_pixels = img_data->output->img_pixels;

	bmp_pixel **final_output_pixels = transpose_matrix(gathered_transposed_pixels, comm_data->dim);
	if (!final_output_pixels) {
		log_error("Rank 0: Failed to transpose output matrix back.");
		bmp_img_pixel_free(gathered_transposed_pixels, comm_data->dim);

		bmp_free_img_spec(img_data);
		free(img_data->input);
		free(img_data->output);
		free(comm_data->dim);
		ABORT_AND_RETURN(-1.0);
	}

	//        bmp_img_pixel_free(gathered_transposed_pixels, comm_data.dim);
	img_data->output->img_pixels = final_output_pixels;
	img_data->output->img_header.biWidth = width;
	img_data->output->img_header.biHeight = height;
	return 0;
}
