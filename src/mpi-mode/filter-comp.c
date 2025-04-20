// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../logger/log.h"
#include "../utils/filters.h"
#include "../utils/threads-general.h"
#include "../mt-mode/compute.h"
#include "mpi-types.h"
#include "filter-comp.h"
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

/**
 * Operates directly on raw byte buffers (`local_data`) using MPI communication
 * geometry (`comm_data`). Reads from the input buffer (including halo rows accessed
 * via calculated local indices) and writes to the output buffer.
 * Handles coordinate translation and uses wrap-around based on global image dimensions.
 *
 * @param local_data - pointer to the structure holding local input/output pixel buffers (unsigned char*).
 * @param comm_data - pointer to the structure holding MPI communication geometry (rows, dimensions, stride).
 * @param cfilter - the filter structure containing the kernel matrix, size, bias, and factor.
 * @param halo_size (default: 1) - the size of the halo region used (needed to calculate offsets correctly, though implicit in comm_data).
 */
static void mpi_apply_filter(const struct mpi_local_data *local_data, const struct img_comm_data *comm_data, struct filter cfilter)
{
	uint32_t x = 0, y = 0;
	int32_t filterX = 0, filterY = 0; // coordinate variables of kernel
	int32_t imageX_global = 0, imageY_global = 0; // global coords in input image
	uint32_t imageY_local = 0;
	uint32_t global_y = 0;
	double weight = 0.0;
	double red_acc = 0, green_acc = 0, blue_acc = 0;
	int padding = cfilter.size / 2;
	const unsigned char *input_pixel_ptr = NULL;
	unsigned char *output_pixel_ptr = NULL;
	const unsigned char *input_row_base = NULL;
	unsigned char *output_row_base = NULL;
	const size_t row_stride = comm_data->row_stride_bytes;
	int32_t potential_imageY_global = 0;

	if (!local_data || !local_data->input_pixels || !local_data->output_pixels || !comm_data || !comm_data->dim || cfilter.size <= 0 || !cfilter.filter_arr) {
		log_error("Rank ?: Invalid arguments passed to mpi_apply_filter. Skipping.");
		return;
	}
	if (comm_data->my_num_rows == 0) {
		log_trace("Rank ?: No rows to process in mpi_apply_filter.");
		return;
	}

	log_debug("halo size %u", comm_data->halo_size);

	log_trace("Rank ?: Applying filter size %d to local region R[%u-%u) C[0-%u) (Output rows)", cfilter.size, comm_data->my_start_row,
		  comm_data->my_start_row + comm_data->my_num_rows, comm_data->dim->width);
	log_debug("Filter size = %d, padding = %d", cfilter.size, padding);

	for (y = 0; y < comm_data->my_num_rows; ++y) {
		output_row_base = local_data->output_pixels + y * row_stride; // Address of yth row start
		global_y = comm_data->my_start_row + y;

		log_debug("y = %u, global_y = %lu", y, global_y);

		for (x = 0; x < comm_data->dim->width; ++x) {
			red_acc = green_acc = blue_acc = 0.0;

			for (filterY = 0; filterY < cfilter.size; ++filterY) {
				for (filterX = 0; filterX < cfilter.size; ++filterX) {
					imageX_global = (x + filterX - padding + comm_data->dim->width) % comm_data->dim->width;
					potential_imageY_global = global_y + filterY - padding;
					log_debug("Pre-Clamp: potential_imageY_global = %d", potential_imageY_global);
					// Better border handling
					if (potential_imageY_global < 0) {
						imageY_global = 0;
					} else if (potential_imageY_global >= comm_data->dim->height) {
						imageY_global = comm_data->dim->height - 1;
					} else {
						imageY_global = potential_imageY_global;
					}
					log_debug("Post-Clamp: imageY_global = %d", imageY_global);
					// comm_data->send_start_row is the global index of the first row in our input buffer
					imageY_local = imageY_global - comm_data->send_start_row; // get local address (in buffer) of current Y

					log_debug("send_start_row %lu my_start_row %lu", comm_data->send_start_row, comm_data->my_start_row);
					log_debug("filter x:y %" PRIu32 ":%" PRIu32 ", imagex %" PRId32 ", imagey %" PRId32 ", local %" PRIu32 "", filterX, filterY, imageX_global,
						  imageY_global, imageY_local);

					if (imageY_local >= comm_data->send_num_rows) {
						log_error("Rank %u: Calc local input row %" PRIu32 " (from global %d) OUT OF BOUNDS [0, %u) for output pixel (%u, %u). Aborting.",
							  comm_data->my_start_row, imageY_local, imageY_global, comm_data->send_num_rows, global_y, x);
						MPI_Abort(MPI_COMM_WORLD, 1);
						return;
					}

					// Start of the array + ammount of el * row_stride (the total number of bytes from the beginning of one row of pixels in memory to the beginning of the next row)
					// Its just a safer and more correct version of the width * BYTES_PER_PIXEL, bc of alignment
					input_row_base = local_data->input_pixels + imageY_local * row_stride;

					// Address of current pixel
					input_pixel_ptr = input_row_base + imageX_global * BYTES_PER_PIXEL;

					weight = cfilter.filter_arr[filterY][filterX];

					blue_acc += input_pixel_ptr[0] * weight;
					green_acc += input_pixel_ptr[1] * weight;
					red_acc += input_pixel_ptr[2] * weight;
				}
			}

			output_pixel_ptr = output_row_base + x * BYTES_PER_PIXEL;

			output_pixel_ptr[0] = (unsigned char)fmin(fmax(round(blue_acc * cfilter.factor + cfilter.bias), 0.0), 255.0);
			output_pixel_ptr[1] = (unsigned char)fmin(fmax(round(green_acc * cfilter.factor + cfilter.bias), 0.0), 255.0);
			output_pixel_ptr[2] = (unsigned char)fmin(fmax(round(red_acc * cfilter.factor + cfilter.bias), 0.0), 255.0);
		}
	}
}

/**
 * Same as `mpi_apply_filter` but for median filter. (see implementation in utils/threads-general)
 *
 * @param local_data - pointer to the structure holding local input/output pixel buffers (unsigned char*).
 * @param comm_data - pointer to the structure holding MPI communication geometry (rows, dimensions, stride).
 * @param filter_size - the size of median filter
 * @param halo_size (default: 1)- the size of the halo region used (needed to calculate offsets correctly, though implicit in comm_data).
 */
static void mpi_apply_median_filter(const struct mpi_local_data *local_data, const struct img_comm_data *comm_data, uint16_t filter_size)
{
	if (filter_size % 2 == 0 || filter_size < 1) {
		log_error("Median filter size must be odd and positive, got %u", filter_size);
		return;
	}
	uint32_t x = 0;
	uint32_t y = 0;
	int32_t half_size = filter_size / 2;
	int32_t filter_area = filter_size * filter_size;
	int32_t *red = NULL, *green = NULL, *blue = NULL;
	int32_t filterX = 0, filterY = 0;
	int32_t imageX_global = 0, imageY_global = 0;
	uint32_t imageY_local = 0;
	uint32_t global_y = 0;
	uint8_t n = 0;
	const unsigned char *input_row_base = NULL;
	const unsigned char *input_pixel_ptr = NULL;
	unsigned char *output_row_base = NULL;
	unsigned char *output_pixel_ptr = NULL;

	const uint32_t width = comm_data->dim->width;
	const uint32_t height = comm_data->dim->height;
	const size_t row_stride = comm_data->row_stride_bytes;

	red = malloc(filter_area * sizeof(*red));
	green = malloc(filter_area * sizeof(*green));
	blue = malloc(filter_area * sizeof(*blue));

	if (!red || !green || !blue) {
		log_error("Failed to allocate memory for median filter arrays.");
		free(red);
		free(green);
		free(blue);
		return;
	}

	log_trace("Rank ?: Applying filter size %d to local region R[%u-%u) C[0-%u) (Output rows)", filter_size, comm_data->my_start_row,
		  comm_data->my_start_row + comm_data->my_num_rows, width);

	for (y = 0; y < comm_data->my_num_rows; y++) {
		output_row_base = local_data->output_pixels + y * row_stride;
		global_y = comm_data->my_start_row + y;

		for (x = 0; x < width; x++) {
			n = 0; // Index for neighborhood arrays

			// Collect neighboring pixel values
			for (filterY = -half_size; filterY <= half_size; filterY++) {
				for (filterX = -half_size; filterX <= half_size; filterX++) {
					imageX_global = (x + filterX + width) % width;
					imageY_global = (y + filterY + height) % height;

					imageY_local = imageY_global - comm_data->send_start_row;

					// See this part apply_filter
					input_row_base = local_data->input_pixels + imageY_local * row_stride;
					input_pixel_ptr = input_row_base + imageX_global * BYTES_PER_PIXEL;

					red[n] = input_pixel_ptr[0];
					green[n] = input_pixel_ptr[1];
					blue[n] = input_pixel_ptr[2];
					n++;
				}
			}
			output_pixel_ptr = output_row_base + x * BYTES_PER_PIXEL;

			// Find the median value for each channel using the K'th smallest element algorithm
			// The median is the element at index filter_area / 2 in the sorted array.
			output_pixel_ptr[0] = (unsigned char)selectKth(red, 0, filter_area, filter_area / 2);
			output_pixel_ptr[1] = (unsigned char)selectKth(green, 0, filter_area, filter_area / 2);
			output_pixel_ptr[2] = (unsigned char)selectKth(blue, 0, filter_area, filter_area / 2);
		}
	}

	free(red);
	free(green);
	free(blue);
}

/**
 * @brief Applies a convolution filter to a region of a TRANSPOSED image buffer.
 * Handles boundary conditions (clamping for X - original vertical,
 * wrap-around for Y - original horizontal) and filter index swapping appropriately.
 *
 * @param local_data Pointer to local MPI data buffers (input contains transposed rows).
 * @param comm_data Pointer to MPI communication geometry (using transposed dimensions).
 * @param cfilter The filter definition.
 */
static void mpi_apply_filter_transposed(const struct mpi_local_data *local_data, const struct img_comm_data *comm_data, struct filter cfilter)
{
	uint32_t x = 0, y = 0;
	int32_t filterX = 0, filterY = 0;
	int32_t imageX_global = 0, imageY_global = 0;
	uint32_t imageY_local = 0; // Local ROW index in input_pixels buffer
	uint32_t global_y = 0; // Global ROW index in TRANSPOSED image
	double weight = 0.0;
	double red_acc = 0, green_acc = 0, blue_acc = 0;
	int padding = cfilter.size / 2;
	const unsigned char *input_pixel_ptr = NULL;
	unsigned char *output_pixel_ptr = NULL;
	const unsigned char *input_row_base = NULL;
	unsigned char *output_row_base = NULL;
	const size_t row_stride = comm_data->row_stride_bytes; // Stride for TRANSPOSED rows
	int32_t potential_imageX_global = 0; // Temp for X coord (original vertical) calculation
	int32_t potential_imageY_global = 0; // Temp for Y coord (original horizontal) calculation

	// Use dimensions from comm_data (which should be transposed dimensions)
	const uint32_t transposed_width = comm_data->dim->width;
	const uint32_t transposed_height = comm_data->dim->height;

	if (!local_data || !local_data->input_pixels || !local_data->output_pixels || !comm_data || !comm_data->dim || cfilter.size <= 0 || !cfilter.filter_arr) {
		log_error("Rank ?: Invalid arguments passed to mpi_apply_filter_transposed. Skipping.");
		return;
	}
	if (comm_data->my_num_rows == 0) {
		log_trace("Rank ?: No rows to process in mpi_apply_filter_transposed.");
		return;
	}

	log_trace("Rank ?: Applying filter size %d to TRANSPOSED local region R[%u-%u) C[0-%u)", cfilter.size, comm_data->my_start_row,
		  comm_data->my_start_row + comm_data->my_num_rows, transposed_width);

	for (y = 0; y < comm_data->my_num_rows; ++y) {
		output_row_base = local_data->output_pixels + y * row_stride;
		global_y = comm_data->my_start_row + y;

		for (x = 0; x < transposed_width; ++x) {
			red_acc = green_acc = blue_acc = 0.0;

			for (filterY = 0; filterY < cfilter.size; ++filterY) {
				for (filterX = 0; filterX < cfilter.size; ++filterX) {
					potential_imageX_global = x + filterX - padding;
					potential_imageY_global = global_y + filterY - padding;

					if (potential_imageX_global < 0) {
						imageX_global = 0;
					} else if (potential_imageX_global >= transposed_width) {
						imageX_global = transposed_width - 1;
					} else {
						imageX_global = potential_imageX_global;
					}

					if (potential_imageY_global < 0) {
                        imageY_global = 0;
                    } else if (potential_imageY_global >= transposed_height) { // Use transposed height
                        imageY_global = transposed_height - 1;
                    } else {
                        imageY_global = potential_imageY_global;
                    }

					imageY_local = imageY_global - comm_data->send_start_row;

					if (imageY_local >= comm_data->send_num_rows) {
						log_error(
							"Rank ? (Transposed): Calc local input row %u (from global Y %d) OUT OF BOUNDS [0, %u) for output pixel (transposed) (%u, %u). Aborting.",
							imageY_local, imageY_global, comm_data->send_num_rows, global_y, x);
						MPI_Abort(MPI_COMM_WORLD, 1);
						return;
					}

					input_row_base = local_data->input_pixels + imageY_local * row_stride;
					input_pixel_ptr = input_row_base + imageX_global * BYTES_PER_PIXEL;

					weight = cfilter.filter_arr[filterX][filterY];

					blue_acc += input_pixel_ptr[0] * weight;
					green_acc += input_pixel_ptr[1] * weight;
					red_acc += input_pixel_ptr[2] * weight;
				}
			}

			output_pixel_ptr = output_row_base + x * BYTES_PER_PIXEL;

			output_pixel_ptr[0] = (unsigned char)fmin(fmax(round(blue_acc * cfilter.factor + cfilter.bias), 0.0), 255.0);
			output_pixel_ptr[1] = (unsigned char)fmin(fmax(round(green_acc * cfilter.factor + cfilter.bias), 0.0), 255.0);
			output_pixel_ptr[2] = (unsigned char)fmin(fmax(round(red_acc * cfilter.factor + cfilter.bias), 0.0), 255.0);
		}
	}
}

void mpi_compute_local_region(const struct mpi_local_data *local_data, const struct img_comm_data *comm_data, const struct p_args *args, const struct filter_mix *filters)
{
	bool is_transposed_mode = (args->compute_mode == BY_COLUMN);

	if (!local_data || !local_data->input_pixels || !local_data->output_pixels || !comm_data || !comm_data->dim || !args || !args->filter_type || !filters) {
		log_error("Rank ?: Invalid NULL parameters in mpi_compute_local_region. Skipping.");
		return;
	}

	if (comm_data->my_num_rows <= 0 || comm_data->dim->width <= 0) {
		log_trace("Rank ?: Skipping local processing due to zero rows/width for rank starting at %u.", comm_data->my_start_row);
		return;
	}

	const char *filter_type = args->filter_type;

	// Dispatch based on filter type AND mode (transposed or not)
	if (strcmp(filter_type, "mb") == 0 && filters->motion_blur) {
		if (is_transposed_mode)
			mpi_apply_filter_transposed(local_data, comm_data, *filters->motion_blur);
		else
			mpi_apply_filter(local_data, comm_data, *filters->motion_blur);
	} else if (strcmp(filter_type, "bb") == 0 && filters->blur) {
		if (is_transposed_mode)
			mpi_apply_filter_transposed(local_data, comm_data, *filters->blur);
		else
			mpi_apply_filter(local_data, comm_data, *filters->blur);
	} else if (strcmp(filter_type, "gb") == 0 && filters->gaus_blur) {
		if (is_transposed_mode)
			mpi_apply_filter_transposed(local_data, comm_data, *filters->gaus_blur);
		else
			mpi_apply_filter(local_data, comm_data, *filters->gaus_blur);
	} else if (strcmp(filter_type, "co") == 0 && filters->conv) {
		if (is_transposed_mode)
			mpi_apply_filter_transposed(local_data, comm_data, *filters->conv);
		else
			mpi_apply_filter(local_data, comm_data, *filters->conv);
	} else if (strcmp(filter_type, "sh") == 0 && filters->sharpen) {
		if (is_transposed_mode)
			mpi_apply_filter_transposed(local_data, comm_data, *filters->sharpen);
		else
			mpi_apply_filter(local_data, comm_data, *filters->sharpen);
	} else if (strcmp(filter_type, "em") == 0 && filters->emboss) {
		if (is_transposed_mode)
			mpi_apply_filter_transposed(local_data, comm_data, *filters->emboss);
		else
			mpi_apply_filter(local_data, comm_data, *filters->emboss);
	} else if (strcmp(filter_type, "mm") == 0) { // Median Filter
		// if (is_transposed_mode) mpi_apply_median_filter_transposed(local_data, comm_data, 15);
		// else mpi_apply_median_filter(local_data, comm_data, 15);
		log_warn("Median filter transpose not fully implemented yet.");
		if (!is_transposed_mode)
			mpi_apply_median_filter(local_data, comm_data, 15);
		else
			MPI_Abort(MPI_COMM_WORLD, 1); // Or handle error appropriately
	} else if (strcmp(filter_type, "gg") == 0 && filters->big_gaus) {
		if (is_transposed_mode)
			mpi_apply_filter_transposed(local_data, comm_data, *filters->big_gaus);
		else
			mpi_apply_filter(local_data, comm_data, *filters->big_gaus);
	} else if (strcmp(filter_type, "bo") == 0 && filters->box_blur) {
		if (is_transposed_mode)
			mpi_apply_filter_transposed(local_data, comm_data, *filters->box_blur);
		else
			mpi_apply_filter(local_data, comm_data, *filters->box_blur);
	} else if (strcmp(filter_type, "mg") == 0 && filters->med_gaus) {
		if (is_transposed_mode)
			mpi_apply_filter_transposed(local_data, comm_data, *filters->med_gaus);
		else
			mpi_apply_filter(local_data, comm_data, *filters->med_gaus);
	} else {
		log_error("Rank ?: Unknown or unsupported filter type '%s' in mpi_process_local_region.", filter_type);
		MPI_Abort(MPI_COMM_WORLD, 1);
	}
}
