// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../libbmp/libbmp.h"
#include "../../logger/log.h"
#include "threads-general.h"
#include "utils.h"
#include "args-parse.h"
#include "filters.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

struct img_dim *init_dimensions(uint16_t width, uint16_t height)
{
	struct img_dim *dim = malloc(sizeof(struct img_dim));
	if (!dim) {
		log_error("Failed to allocate memory for img_dim.");
		return NULL;
	}
	dim->width = width;
	dim->height = height;
	log_debug("Initialized dimensions: Width=%u, Height=%u", width, height);
	return dim;
}

struct img_spec *init_img_spec(bmp_img *input, bmp_img *output)
{
	struct img_spec *spec = malloc(sizeof(struct img_spec));
	if (!spec) {
		log_error("Failed to allocate memory for img_spec.");
		return NULL;
	}
	spec->input_img = input;
	spec->output_img = output;
	return spec;
}

void *init_thread_spec(struct p_args *args, struct filter_mix *filters)
{
	struct thread_spec *th_spec = malloc(sizeof(struct thread_spec));
	if (!th_spec) {
		log_error("Failed to allocate memory for thread_spec.");
		return NULL;
	}

	th_spec->dim = NULL;
	th_spec->img = NULL;
	th_spec->start_row = 0;
	th_spec->end_row = 0;
	th_spec->start_column = 0;
	th_spec->end_column = 0;

	struct sthreads_gen_info *st_gen_info = malloc(sizeof(struct sthreads_gen_info));
	if (!st_gen_info) {
		free(th_spec);
		log_error("Failed to allocate memory for thread_spec.");
		return NULL;
	}

	st_gen_info->args = args;
	st_gen_info->filters = filters;

	th_spec->st_gen_info = st_gen_info;

	return th_spec;
}

void apply_filter(struct thread_spec *spec, struct filter cfilter)
{
	int32_t x, y, filterX, filterY, imageX, imageY;
	double weight = 0;
	bmp_pixel orig_pixel;
	double red_acc, green_acc, blue_acc;
	int padding = cfilter.size / 2;
	int32_t potential_imageY = 0, potential_imageX = 0;

	log_trace("Applying filter size %d to region R[%d-%d) C[%d-%d)", cfilter.size, spec->start_row, spec->end_row, spec->start_column, spec->end_column);

	for (y = spec->start_row; y < spec->end_row; y++) {
		for (x = spec->start_column; x < spec->end_column; x++) {
			red_acc = 0.0;
			green_acc = 0.0;
			blue_acc = 0.0;

			// Apply the filter kernel
			for (filterY = 0; filterY < cfilter.size; filterY++) {
				for (filterX = 0; filterX < cfilter.size; filterX++) {
					potential_imageX = x + filterX - padding;
					if (potential_imageX < 0) {
						imageX = 0;
					} else if (potential_imageX >= spec->dim->width) {
						imageX = spec->dim->width - 1;
					} else {
						imageX = potential_imageX;
					}

					potential_imageY = y + filterY - padding;
					if (potential_imageY < 0) {
						imageY = 0;
					} else if (potential_imageY >= spec->dim->height) {
						imageY = spec->dim->height - 1;
					} else {
						imageY = potential_imageY;
					}

					if (imageY < 0 || imageY >= spec->dim->height || imageX < 0 || imageX >= spec->dim->width) {
						log_error("apply_filter: Calculated index out of bounds after clamping! Y=%d (H=%d), X=%d (W=%d)", imageY, spec->dim->height,
							  imageX, spec->dim->width);
						continue;
					}
					orig_pixel = spec->img->input_img->img_pixels[imageY][imageX];
					weight = cfilter.filter_arr[filterY][filterX];

					red_acc += orig_pixel.red * weight;
					green_acc += orig_pixel.green * weight;
					blue_acc += orig_pixel.blue * weight;
				}
			}

			// Apply factor, bias, clamp to [0, 255], and cast to output type
			spec->img->output_img->img_pixels[y][x].red = (unsigned char)fmin(fmax(round(red_acc * cfilter.factor + cfilter.bias), 0.0), 255.0);
			spec->img->output_img->img_pixels[y][x].green = (unsigned char)fmin(fmax(round(green_acc * cfilter.factor + cfilter.bias), 0.0), 255.0);
			spec->img->output_img->img_pixels[y][x].blue = (unsigned char)fmin(fmax(round(blue_acc * cfilter.factor + cfilter.bias), 0.0), 255.0);
		}
	}
}

void apply_median_filter(struct thread_spec *spec, uint16_t filter_size)
{
	if (filter_size % 2 == 0 || filter_size < 1) {
		log_error("Median filter size must be odd and positive, got %u", filter_size);
		return;
	}
	int32_t half_size = filter_size / 2;
	int32_t filter_area = filter_size * filter_size;
	int32_t *red = NULL, *green = NULL, *blue = NULL;

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

	log_trace("Applying median filter size %u to region R[%d-%d) C[%d-%d)", filter_size, spec->start_row, spec->end_row, spec->start_column, spec->end_column);

	for (int y = spec->start_row; y < spec->end_row; y++) {
		for (int x = spec->start_column; x < spec->end_column; x++) {
			int n = 0; // Index for neighborhood arrays

			// Collect neighboring pixel values
			for (int filterY = -half_size; filterY <= half_size; filterY++) {
				for (int filterX = -half_size; filterX <= half_size; filterX++) {
					int imageX = (x + filterX + spec->dim->width) % spec->dim->width;
					int imageY = (y + filterY + spec->dim->height) % spec->dim->height;

					bmp_pixel orig_pixel = spec->img->input_img->img_pixels[imageY][imageX];

					red[n] = orig_pixel.red;
					green[n] = orig_pixel.green;
					blue[n] = orig_pixel.blue;
					n++;
				}
			}

			// Find the median value for each channel using the K'th smallest element algorithm
			// The median is the element at index filter_area / 2 in the sorted array.
			spec->img->output_img->img_pixels[y][x].red = (unsigned char)selectKth(red, 0, filter_area, filter_area / 2);
			spec->img->output_img->img_pixels[y][x].green = (unsigned char)selectKth(green, 0, filter_area, filter_area / 2);
			spec->img->output_img->img_pixels[y][x].blue = (unsigned char)selectKth(blue, 0, filter_area, filter_area / 2);
		}
	}

	free(red);
	free(green);
	free(blue);
}

void filter_part_computation(struct thread_spec *spec, char *filter_type, struct filter_mix *filters)
{
	if (!filter_type || !filters || !spec) {
		log_error("NULL parameter passed to filter_part_computation.");
		return;
	}

	if (strcmp(filter_type, "mb") == 0) {
		apply_filter(spec, *filters->motion_blur);
	} else if (strcmp(filter_type, "bb") == 0) {
		apply_filter(spec, *filters->blur);
	} else if (strcmp(filter_type, "gb") == 0) {
		apply_filter(spec, *filters->gaus_blur);
	} else if (strcmp(filter_type, "co") == 0) {
		apply_filter(spec, *filters->conv);
	} else if (strcmp(filter_type, "sh") == 0) {
		apply_filter(spec, *filters->sharpen);
	} else if (strcmp(filter_type, "em") == 0) {
		apply_filter(spec, *filters->emboss);
	} else if (strcmp(filter_type, "mm") == 0) { // Median Filter
		apply_median_filter(spec, 15); // Using fixed size 15x15 for "mm"
	} else if (strcmp(filter_type, "gg") == 0) {
		apply_filter(spec, *filters->big_gaus);
	} else if (strcmp(filter_type, "bo") == 0) {
		apply_filter(spec, *filters->box_blur);
	} else if (strcmp(filter_type, "mg") == 0) {
		apply_filter(spec, *filters->med_gaus);
	} else {
		log_error("Unknown filter type parameter '%s' in filter_part_computation.", filter_type);
	}
}

void save_result_image(char *output_filepath, size_t path_len, int threadnum, bmp_img *img_result, const struct p_args *args)
{
	int8_t status = 0;
	if (strcmp(args->output_filename, "") != 0) {
		snprintf(output_filepath, path_len, "test-img/%s", args->output_filename);
	} else if (args->mt_mode == 2) {
		snprintf(output_filepath, path_len, "test-img/mpi_out_%s", args->input_filename[0]);
	} else {
		if (threadnum > 1) {
			snprintf(output_filepath, path_len, "test-img/rcon_out_%s", args->input_filename[0]);
		} else if (threadnum == 1) {
			snprintf(output_filepath, path_len, "test-img/seq_out_%s", args->input_filename[0]);
		}
	}

	log_debug("Result out filepath %s\n", output_filepath);
	if (!img_result->img_pixels)
		log_error("Pointer to images pixel array is NULL");
	status = bmp_img_write(img_result, output_filepath);

	log_debug("status %d", status);
}

void free_img_spec(struct img_spec *img_data)
{
	free(img_data->input_img);
	free(img_data->output_img);
}

void bmp_free_img_spec(struct img_spec *img_data)
{
	bmp_img_free(img_data->input_img);
	bmp_img_free(img_data->output_img);
}

void bmp_img_pixel_free(bmp_pixel **pixels_to_free, const struct img_dim *original_dim)
{
	uint32_t i = 0, num_allocated_rows = 0;

	if (!pixels_to_free || !original_dim)
		return;

	num_allocated_rows = original_dim->width;

	for (i = 0; i < num_allocated_rows; i++) {
		free(pixels_to_free[i]);
		pixels_to_free[i] = NULL;
	}

	free(pixels_to_free);
}

bmp_pixel **transpose_matrix(bmp_pixel **img_pixels, const struct img_dim *dim)
{
	if (!img_pixels || !dim) {
		return NULL;
	}
	uint32_t original_height = dim->height;
	uint32_t original_width = dim->width;

	if (original_height == 0 || original_width == 0) {
		return NULL;
	}

	bmp_pixel **transposed_matrix = bmp_img_pixel_alloc(original_width, original_height); // Allocate W rows, H columns
	if (!transposed_matrix) {
		return NULL;
	}

	for (uint32_t y = 0; y < original_height; ++y) { // Iterate rows of original (0 to H-1)
		if (!img_pixels[y]) {
			bmp_img_pixel_free(transposed_matrix, dim); // Free using the height it was allocated with (W)
			return NULL;
		}
		for (uint32_t x = 0; x < original_width; ++x) {
			transposed_matrix[x][y] = img_pixels[y][x];
		}
	}

	return transposed_matrix;
}
