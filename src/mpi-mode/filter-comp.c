
#include <stdint.h>
#include "filter-comp.h"

/**
 * @brief Applies a convolution filter (MPI-adapted) to the local data chunk.
 *
 * Operates directly on raw byte buffers (`local_data`) using MPI communication
 * geometry (`comm_data`). Reads from the input buffer (including halo rows accessed
 * via calculated local indices) and writes to the output buffer (core rows only).
 * Handles coordinate translation and uses wrap-around based on global image dimensions.
 *
 * @param local_data Pointer to the structure holding local input/output pixel buffers (unsigned char*).
 * @param comm_data Pointer to the structure holding MPI communication geometry (rows, dimensions, stride).
 * @param cfilter The filter structure containing the kernel matrix, size, bias, and factor.
 * @param halo_size The size of the halo region used (needed to calculate offsets correctly, though implicit in comm_data).
 */
static void mpi_apply_filter(const struct mpi_local_data *local_data,
                             const struct img_comm_data *comm_data,
                             struct filter cfilter,
                             uint32_t halo_size) // Pass halo_size explicitly
{
    int32_t x; // Column index (0 to width-1)
    uint32_t y; // LOCAL output row index (0 to my_num_rows-1)
    int32_t filterX, filterY; // Kernel loop indices
    int32_t imageX_global, imageY_global; // GLOBAL source coordinates after wrap-around
    uint32_t imageY_local; // LOCAL input row index (within input_pixels buffer)
    uint32_t global_y; // GLOBAL output row index
    double weight = 0.0;
    double red_acc, green_acc, blue_acc;
    int padding = cfilter.size / 2;
    const unsigned char *input_pixel_ptr = NULL;
    unsigned char *output_pixel_ptr = NULL;
    const unsigned char *input_row_base = NULL; // Base address of a specific row in input buffer
    unsigned char *output_row_base = NULL; // Base address of a specific row in output buffer
    const uint32_t width = comm_data->dim->width;
    const uint32_t height = comm_data->dim->height;
    const size_t row_stride = comm_data->row_stride_bytes;

    // --- Input Validation ---
    if (!local_data || !local_data->input_pixels || !local_data->output_pixels ||
        !comm_data || !comm_data->dim || cfilter.size <= 0 || !cfilter.filter_arr)
    {
        log_error("Rank ?: Invalid arguments passed to mpi_apply_filter. Skipping.");
        // Consider MPI_Abort for critical failures if needed
        return;
    }
     if (comm_data->my_num_rows == 0) {
         log_trace("Rank ?: No rows to process in mpi_apply_filter.");
         return;
     }
    // --- End Input Validation ---


	log_trace("Rank ?: Applying filter size %d to local region R[%u-%u) C[0-%u) (Output rows)",
              cfilter.size, comm_data->my_start_row, comm_data->my_start_row + comm_data->my_num_rows, width);

    // Iterate over the rows this rank is responsible for *computing* (local output rows)
	for (y = 0; y < comm_data->my_num_rows; ++y) {
        // Base pointer for the current output row
        output_row_base = local_data->output_pixels + y * row_stride;
        // Calculate the corresponding global row index for source calculations
        global_y = comm_data->my_start_row + y;

        // Iterate over all columns
		for (x = 0; x < width; ++x) {
			red_acc = 0.0;
			green_acc = 0.0;
			blue_acc = 0.0;

			// Apply the filter kernel
			for (filterY = 0; filterY < cfilter.size; ++filterY) {
				for (filterX = 0; filterX < cfilter.size; ++filterX) {
					// 1. Calculate GLOBAL source pixel coordinates with wrap-around
					imageX_global = (x + filterX - padding + width) % width;
					imageY_global = (global_y + filterY - padding + height) % height;

                    // 2. Translate GLOBAL input Y to LOCAL input row index
                    //    (relative to the start of local_data->input_pixels buffer)
                    //    comm_data->send_start_row is the global index of the first row in our input buffer
                    imageY_local = imageY_global - comm_data->send_start_row;

                    // --- Sanity Check (CRITICAL) ---
                    // Check if the calculated local row index is within the bounds of the received data
                    // (0 to send_num_rows - 1)
                    if (imageY_local >= comm_data->send_num_rows) {
                         log_error("Rank %u: Calc local input row %u (from global %d) OUT OF BOUNDS [0, %u) for output pixel (%u, %u). Aborting.",
                                   comm_data->my_start_row, // Use start_row as approximation for rank in logs
                                   imageY_local, imageY_global, comm_data->send_num_rows,
                                   global_y, x);
                         // This usually indicates an error in halo exchange or row distribution logic
                         MPI_Abort(MPI_COMM_WORLD, 1);
                         return; // Should not be reached after Abort
                    }
                    // --- End Sanity Check ---

                    // 3. Get base pointer for the required input row in the local buffer
                    input_row_base = local_data->input_pixels + imageY_local * row_stride;

                    // 4. Get pointer to the specific input pixel bytes (using GLOBAL X)
                    input_pixel_ptr = input_row_base + imageX_global * BYTES_PER_PIXEL;

					weight = cfilter.filter_arr[filterY][filterX];

                    // 5. Accumulate weighted values (Assuming BGR byte order common in BMP)
                    //    Adjust indices if your format is RGB (0=R, 1=G, 2=B)
					blue_acc  += input_pixel_ptr[0] * weight; // Index 0 = Blue
					green_acc += input_pixel_ptr[1] * weight; // Index 1 = Green
					red_acc   += input_pixel_ptr[2] * weight; // Index 2 = Red
				}
			}

            // 6. Get pointer to the current output pixel bytes
            output_pixel_ptr = output_row_base + x * BYTES_PER_PIXEL;

			// 7. Apply factor, bias, clamp to [0, 255], and store result (assuming BGR output)
            //    Adjust indices if your format is RGB
			output_pixel_ptr[0] = (unsigned char)fmin(fmax(round(blue_acc * cfilter.factor + cfilter.bias), 0.0), 255.0); // Blue
			output_pixel_ptr[1] = (unsigned char)fmin(fmax(round(green_acc * cfilter.factor + cfilter.bias), 0.0), 255.0); // Green
			output_pixel_ptr[2] = (unsigned char)fmin(fmax(round(red_acc * cfilter.factor + cfilter.bias), 0.0), 255.0);   // Red
		}
	}
}


/**
 * @brief Processes the local image region assigned to the current MPI rank.
 *
 * This function selects and applies the appropriate image filter (convolution or median)
 * to the portion of the image data held locally by this process, operating on
 * continuous raw byte buffers.
 *
 * @param local_data Pointer to the structure holding local input/output pixel buffers (unsigned char*).
 * @param comm_data Pointer to the structure holding MPI communication geometry for this rank.
 * @param filter_type String identifier for the desired filter (e.g., "mb", "mm", "sh").
 * @param filters Pointer to the structure containing pre-initialized filter kernels.
 * @param halo_size The size of the halo (ghost rows) included in the input buffer.
 */
static void mpi_process_local_region(const struct mpi_local_data *local_data,
                                     const struct img_comm_data *comm_data, // Use pointer consistently
                                     const char* filter_type, // Use const char*
                                     const struct filter_mix *filters,
                                     uint32_t halo_size)
{
    // Basic validation moved inside specific filter functions, but keep top-level check
    if (!local_data || !local_data->input_pixels || !local_data->output_pixels ||
        !comm_data || !comm_data->dim || !filter_type || !filters)
    {
        log_error("Rank ?: Invalid NULL parameters in mpi_process_local_region. Skipping.");
        return; // Or Abort depending on severity
    }

    if (comm_data->my_num_rows <= 0 || comm_data->dim->width <= 0) {
        log_trace("Rank ?: Skipping local processing due to zero rows/width for rank starting at %u.", comm_data->my_start_row);
        return;
    }

    // Dispatch based on filter type
	if (strcmp(filter_type, "mb") == 0 && filters->motion_blur) {
		mpi_apply_filter(local_data, comm_data, *filters->motion_blur, halo_size);
	} else if (strcmp(filter_type, "bb") == 0 && filters->blur) {
		mpi_apply_filter(local_data, comm_data, *filters->blur, halo_size);
	} else if (strcmp(filter_type, "gb") == 0 && filters->gaus_blur) {
		mpi_apply_filter(local_data, comm_data, *filters->gaus_blur, halo_size);
	} else if (strcmp(filter_type, "co") == 0 && filters->conv) {
        mpi_apply_filter(local_data, comm_data, *filters->conv, halo_size);
    } else if (strcmp(filter_type, "sh") == 0 && filters->sharpen) {
		mpi_apply_filter(local_data, comm_data, *filters->sharpen, halo_size);
	} else if (strcmp(filter_type, "em") == 0 && filters->emboss) {
		mpi_apply_filter(local_data, comm_data, *filters->emboss, halo_size);
	} else if (strcmp(filter_type, "mm") == 0) { // Median Filter
        // Assuming mpi_apply_median_filter exists and is correctly adapted for MPI buffers
		mpi_apply_median_filter(local_data, comm_data, 15, halo_size); // Using fixed size 15x15 for "mm"
	} else if (strcmp(filter_type, "gg") == 0 && filters->big_gaus) {
        mpi_apply_filter(local_data, comm_data, *filters->big_gaus, halo_size);
    } else if (strcmp(filter_type, "bo") == 0 && filters->box_blur) {
        mpi_apply_filter(local_data, comm_data, *filters->box_blur, halo_size);
    } else if (strcmp(filter_type, "mg") == 0 && filters->med_gaus) {
        mpi_apply_filter(local_data, comm_data, *filters->med_gaus, halo_size);
    } else {
        // Check if the filter pointer itself was NULL if expected
        if ((strcmp(filter_type, "mb") == 0 && !filters->motion_blur) ||
            (strcmp(filter_type, "bb") == 0 && !filters->blur)   ||
            /* ... add similar checks for all filters accessed via filters->... */
            (strcmp(filter_type, "mg") == 0 && !filters->med_gaus)) {
             log_error("Rank ?: Filter type '%s' requested, but corresponding filter data is NULL.", filter_type);
        } else {
		    log_error("Rank ?: Unknown or unsupported filter type '%s' in mpi_process_local_region.", filter_type);
        }
        // Consider aborting for unknown/unsupported filter types
        MPI_Abort(MPI_COMM_WORLD, 1);
	}
}
