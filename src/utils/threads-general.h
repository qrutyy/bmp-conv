// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdlib.h>
#include "libbmp/libbmp.h"
#include "utils.h"
#include "args-parse.h"
#include "filters.h"

// thread-specific parameters for computation only.
struct thread_spec {
	struct img_spec *img;
	struct st_gen_info *st_gen_info;
	uint16_t start_column;
	uint16_t start_row;
	uint16_t end_row;
	uint16_t end_column;
};

// i know that isn't necessary, just a way to make it cleaner
// (somewhere without 6 '->'), just an alias
struct img_dim {
	uint16_t height;
	uint16_t width;
};

// since thread manages only 1 image computation -> this struct is used.
struct img_spec {
	bmp_img *input;
	bmp_img *output;

	struct img_dim *dim;
};

// simple threads general info
struct st_gen_info {
	struct p_args *args;
	struct filter_mix *filters;
};

bmp_img *setup_input_file(struct p_args *args);
struct img_spec *setup_img_spec(struct p_args *args);
struct filter_mix *setup_filters(struct p_args *args);

/**
 * Allocates and initializes an image dimensions structure.
 *
 * @param width The width of the image in pixels.
 * @param height The height of the image in pixels.
 * @return Pointer to the newly allocated img_dim structure, or NULL on failure.
 */
struct img_dim *init_dimensions(uint16_t width, uint16_t height);

/**
 * Allocates and initializes an image specification structure, linking input and output image buffers.
 *
 * @param input Pointer to the bmp_img structure holding the input image data.
 * @param output Pointer to the bmp_img structure where the output image data will be stored.
 * @param output Pointer to the img_dim structure containing size details.
 * @return Pointer to the newly allocated img_spec structure, or NULL on failure.
 */
struct img_spec *init_img_spec(bmp_img *input, bmp_img *output, struct img_dim *dim);

/**
 * Allocates memory for a thread specification structure. Note: This basic version only allocates the structure. Further initialization (linking dimensions, images, setting row/column ranges) happens elsewhere.
 *
 * @param args Pointer to the p_args structure (potentially unused in this basic init).
 * @param filters Pointer to the filter_mix structure (potentially unused in this basic init).
 * @return Pointer to the newly allocated thread_spec structure, or NULL on failure.
 */
void *init_thread_spec(struct p_args *args, struct filter_mix *filters);

/**
 * Applies a convolution filter (defined by `cfilter`) to a specified portion of an image.
 * Iterates through the pixel range defined in `spec` (start/end row/column).
 * For each pixel, it calculates the weighted sum of neighboring pixels based on
 * the filter kernel, applies bias and factor, clamps the result to [0, 255],
 * and stores it in the output image buffer.
 * Uses wrap-around (modulo) for horizontal boundary handling and
 * clamping for vertical boundary handling (to match MPI implementation).
 *
 * !NOTE:
 * This filter application algorithm is using clamping (rounding to the border) method.
 * That means, that required out-of-bounds elements aren't being accesssed. Instead - we access the border ones.
 * As you may know - there is a wrap-around technique, that redirects out-of-bound elements to the opposite side.
 * We aren't using it, due to difficulty of MPI version implementation.
 * It causes a lot of auxiliary stages for data transfering. (halos)
 *
 * @param spec Pointer to the thread_spec structure containing image data, dimensions,
 *             and the specific row/column range to process.
 * @param cfilter The filter structure containing the kernel matrix, size, bias, and factor.
 */
void apply_filter(struct thread_spec *spec, struct filter cfilter);

/**
 * Applies a median filter of a given square size to a specified portion of an image. Iterates through the pixel range defined in `spec`. For each pixel, it collects the color channel values (Red, Green, Blue) of its neighbors within the filter window, finds the median value for each channel using `selectKth`, and stores the median values in the output image buffer. Uses wrap-around for boundary handling.
 *
 * @param spec Pointer to the thread_spec structure containing image data, dimensions, and the specific row/column range to process.
 * @param filter_size The dimension (width and height) of the square median filter window (e.g., 3 for 3x3).
 */
void apply_median_filter(struct thread_spec *spec, uint16_t filter_size);

/**
 * Selects and applies the appropriate filter based on the filter_type string. Compares filter_type against known filter identifiers and calls either `apply_filter` (for convolution filters) or `apply_median_filter`.
 *
 * @param spec Pointer to the thread_spec structure containing image data and processing range.
 * @param filter_type A string identifier for the desired filter (e.g., "mb", "mm", "sh").
 * @param filters Pointer to the filter_mix structure containing pre-initialized filter data.
 * @return void. Calls the relevant filter application function.
 */
void filter_part_computation(struct thread_spec *spec, char *filter_type, struct filter_mix *filters);

void save_result_image(char *output_filepath, size_t path_len, int threadnum, bmp_img *img_result, const struct p_args *args);
void free_img_spec(struct img_spec *img_data);
void bmp_free_img_spec(struct img_spec *img_data);
void bmp_img_pixel_free(bmp_pixel **pixels_to_free, const struct img_dim *original_dim);
bmp_pixel **transpose_matrix(bmp_pixel **img_pixels, const struct img_dim *dim);
