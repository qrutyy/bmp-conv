// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../logger/log.h"
#include "../utils/filters.h"
#include "../utils/threads-general.h"
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
 * Works in transposition cases as well.
 *
 * For clamping description - see apply_filter at utils/threads-general.c
 *
 * @param local_data - pointer to the structure holding local input/output pixel buffers (unsigned char*).
 * @param comm_data - pointer to the structure holding MPI communication geometry (rows, dimensions, stride).
 * @param cfilter - the filter structure containing the kernel matrix, size, bias, and factor.
 * @param halo_size (default: 1) - the size of the halo region used (needed to calculate offsets correctly, though implicit in comm_data).
 */
static void mpi_apply_filter(const struct mpi_local_data *local_data, const struct img_comm_data *comm_data, struct filter cfilter, int8_t rank)
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
	int32_t potential_imageY_global = 0, potential_imageX_global = 0;

	if (!local_data || !local_data->input_pixels || !local_data->output_pixels || !comm_data || !comm_data->dim || cfilter.size <= 0 || !cfilter.filter_arr) {
		log_error("Rank ?: Invalid arguments passed to mpi_apply_filter. Skipping.");
		return;
	}
	if (comm_data->my_num_rc == 0) {
		log_trace("Rank ?: No rows to process in mpi_apply_filter.");
		return;
	}

	log_trace("Rank ?: Applying filter size %d to local region R[%u-%u) C[0-%u) (Output rows)", cfilter.size, comm_data->my_start_rc,
		  comm_data->my_start_rc + comm_data->my_num_rc, comm_data->dim->width);
	log_debug("Filter size = %d, padding = %d", cfilter.size, padding);

	for (y = 0; y < comm_data->my_num_rc; ++y) {
		output_row_base = local_data->output_pixels + y * row_stride; // Address of yth row start
		global_y = comm_data->my_start_rc + y;

		for (x = 0; x < comm_data->dim->width; ++x) {
			red_acc = green_acc = blue_acc = 0.0;

			for (filterY = 0; filterY < cfilter.size; ++filterY) {
				for (filterX = 0; filterX < cfilter.size; ++filterX) {
					potential_imageX_global = x + filterX - padding;
					if (potential_imageX_global < 0) {
						imageX_global = 0;
					} else if (potential_imageX_global >= comm_data->dim->width) {
						imageX_global = comm_data->dim->width - 1;
					} else {
						imageX_global = potential_imageX_global;
					}

					potential_imageY_global = global_y + filterY - padding;
					if (potential_imageY_global < 0) {
						imageY_global = 0;
					} else if (potential_imageY_global >= comm_data->dim->height) {
						imageY_global = comm_data->dim->height - 1;
					} else {
						imageY_global = potential_imageY_global;
					}
					//log_debug("Post-Clamp: imageY_global = %d", imageY_global); - these logs overload processes -> use only when needed
					// comm_data->send_start_rc is the global index of the first row in our input buffer
					imageY_local = imageY_global - comm_data->send_start_rc; // get local address (in buffer) of current Y
					// log_debug("Rank %u: send_start_rc %lu my_start_rc %lu", rank, comm_data->send_start_rc, comm_data->my_start_rc);
					// log_debug("Rank %u: Matrix x:y %" PRIu32 ":%" PRIu32 ", imagex %" PRId32 ", imagey %" PRId32 ", local %" PRIu32 "", rank, filterX, filterY, imageX_global,
					//			  imageY_global, imageY_local);

					if (imageY_local >= comm_data->send_num_rc || imageY_local < 0) {
						log_error("Rank %u: Calc local input row %" PRIu32 " (from global %d) OUT OF BOUNDS [0, %u) for output pixel (%u, %u). Aborting.",
							  comm_data->my_start_rc, imageY_local, imageY_global, comm_data->send_num_rc, global_y, x);
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
	uint32_t x = 0, y = 0;
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

	log_trace("Rank ?: Applying filter size %d to local region R[%u-%u) C[0-%u) (Output rows)", filter_size, comm_data->my_start_rc,
		  comm_data->my_start_rc + comm_data->my_num_rc, width);

	for (y = 0; y < comm_data->my_num_rc; y++) {
		output_row_base = local_data->output_pixels + y * row_stride;
		global_y = comm_data->my_start_rc + y;

		for (x = 0; x < width; x++) {
			n = 0; // Index for neighborhood arrays

			// Collect neighboring pixel values
			for (filterY = -half_size; filterY <= half_size; filterY++) {
				for (filterX = -half_size; filterX <= half_size; filterX++) {
					imageX_global = (x + filterX + width) % width;
					imageY_global = (y + filterY + height) % height;

					imageY_local = imageY_global - comm_data->send_start_rc;

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

void mpi_compute_local_region(const struct mpi_local_data *local_data, const struct img_comm_data *comm_data, const struct p_args *args, const struct filter_mix *filters,
			      const struct mpi_context *ctx)
{
	if (!local_data || !local_data->input_pixels || !local_data->output_pixels || !comm_data || !comm_data->dim || !args || !args->filter_type || !filters) {
		log_error("Rank ?: Invalid NULL parameters in mpi_compute_local_region. Skipping.");
		return;
	}

	if (comm_data->my_num_rc <= 0 || comm_data->dim->width <= 0) {
		log_trace("Rank ?: Skipping local processing due to zero rows/width for rank starting at %u.", comm_data->my_start_rc);
		return;
	}

	const char *filter_type = args->filter_type;

	// Dispatch based on filter type AND mode (transposed or not)
	if (strcmp(filter_type, "mb") == 0 && filters->motion_blur) {
		mpi_apply_filter(local_data, comm_data, *filters->motion_blur, ctx->rank);
	} else if (strcmp(filter_type, "bb") == 0 && filters->blur) {
		mpi_apply_filter(local_data, comm_data, *filters->blur, ctx->rank);
	} else if (strcmp(filter_type, "gb") == 0 && filters->gaus_blur) {
		mpi_apply_filter(local_data, comm_data, *filters->gaus_blur, ctx->rank);
	} else if (strcmp(filter_type, "co") == 0 && filters->conv) {
		mpi_apply_filter(local_data, comm_data, *filters->conv, ctx->rank);
	} else if (strcmp(filter_type, "sh") == 0 && filters->sharpen) {
		mpi_apply_filter(local_data, comm_data, *filters->sharpen, ctx->rank);
	} else if (strcmp(filter_type, "em") == 0 && filters->emboss) {
		mpi_apply_filter(local_data, comm_data, *filters->emboss, ctx->rank);
	} else if (strcmp(filter_type, "mm") == 0) { // Median Filter
		log_warn("Median filter transpose not fully implemented yet.");
		mpi_apply_median_filter(local_data, comm_data, 15);
	} else if (strcmp(filter_type, "gg") == 0 && filters->big_gaus) {
		mpi_apply_filter(local_data, comm_data, *filters->big_gaus, ctx->rank);
	} else if (strcmp(filter_type, "bo") == 0 && filters->box_blur) {
		mpi_apply_filter(local_data, comm_data, *filters->box_blur, ctx->rank);
	} else if (strcmp(filter_type, "mg") == 0 && filters->med_gaus) {
		mpi_apply_filter(local_data, comm_data, *filters->med_gaus, ctx->rank);
	} else {
		log_error("Rank ?: Unknown or unsupported filter type '%s' in mpi_process_local_region.", filter_type);
		MPI_Abort(MPI_COMM_WORLD, 1);
	}
}
