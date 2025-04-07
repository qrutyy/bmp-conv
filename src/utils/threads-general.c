// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../libbmp/libbmp.h"
#include "../../logger/log.h"
#include "threads-general.h"
#include "utils.h"
#include "args-parse.h"
#include "filters.h"
#include <string.h>
#include <math.h>

struct img_dim *init_dimensions(uint16_t width, uint16_t height)
{
	struct img_dim *dim = malloc(sizeof(struct img_dim));
	if (!dim)
		return NULL;
	dim->width = width;
	dim->height = height;
	log_debug("Width: %d, Height: %d\n", width, height);
	return dim;
}

struct img_spec *init_img_spec(bmp_img *input, bmp_img *output)
{
	struct img_spec *spec = malloc(sizeof(struct img_spec));
	if (!spec)
		return NULL;
	spec->input_img = input;
	spec->output_img = output;
	return spec;
}

void *init_thread_spec(struct p_args *args, struct filter_mix *filters)
{
	struct thread_spec *th_spec = malloc(sizeof(struct thread_spec));
	if (!th_spec)
		return NULL;

	return th_spec;
}

void apply_filter(struct thread_spec *spec, struct filter cfilter)
{
	int32_t x, y, filterX, filterY, imageX, imageY, weight = 0;
	bmp_pixel orig_pixel;
	//	printf("filter size %d start x:%d y:%d, end x:%d y:%d\n", cfilter.size, spec->start_column, spec->start_row, spec->end_column, spec->end_row);

	for (y = spec->start_row; y < spec->end_row; y++) {
		for (x = spec->start_column; x < spec->end_column; x++) {
			int red = 0, green = 0, blue = 0;

			for (filterY = 0; filterY < cfilter.size; filterY++) {
				for (filterX = 0; filterX < cfilter.size; filterX++) {
					imageX = (x + filterX - PADDING + spec->dim->width) % spec->dim->width;
					imageY = (y + filterY - PADDING + spec->dim->height) % spec->dim->height;

					// Check if the pixel is within bounds
					if (imageX >= 0 && imageX < spec->dim->width && imageY >= 0 && imageY < spec->dim->height) {
						orig_pixel = spec->img->input_img->img_pixels[imageY][imageX];
						weight = cfilter.filter_arr[filterY][filterX];

						// Multiply the pixel value with the filter weight
						red += orig_pixel.red * weight;
						green += orig_pixel.green * weight;
						blue += orig_pixel.blue * weight;
					}
				}
			}

			spec->img->output_img->img_pixels[y][x].red = fmin(fmax((int)(red * cfilter.factor + cfilter.bias), 0), 255);
			spec->img->output_img->img_pixels[y][x].green = fmin(fmax((int)(green * cfilter.factor + cfilter.bias), 0), 255);
			spec->img->output_img->img_pixels[y][x].blue = fmin(fmax((int)(blue * cfilter.factor + cfilter.bias), 0), 255);
		}
	}
}

void apply_median_filter(struct thread_spec *spec, uint16_t filter_size)
{
	int32_t half_size = filter_size / 2;
	int32_t filter_area = filter_size * filter_size;

	int32_t *red = malloc(filter_area * sizeof(int));
	int32_t *green = malloc(filter_area * sizeof(int));
	int32_t *blue = malloc(filter_area * sizeof(int));

	for (int y = spec->start_row; y < spec->end_row; y++) {
		for (int x = spec->start_column; x < spec->end_column; x++) {
			int n = 0;

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

			// Apply median filter using selectKth to get the middle value
			spec->img->output_img->img_pixels[y][x].red = selectKth(red, 0, filter_area, filter_area / 2);
			spec->img->output_img->img_pixels[y][x].green = selectKth(green, 0, filter_area, filter_area / 2);
			spec->img->output_img->img_pixels[y][x].blue = selectKth(blue, 0, filter_area, filter_area / 2);
		}
	}

	free(red);
	free(green);
	free(blue);
}

void filter_part_computation(struct thread_spec *spec, char *filter_type, struct filter_mix *filters)
{
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
	} else if (strcmp(filter_type, "mm") == 0) {
		apply_median_filter(spec, 15);
	} else if (strcmp(filter_type, "gg") == 0) {
		apply_filter(spec, *filters->big_gaus);
	} else if (strcmp(filter_type, "bo") == 0) {
		apply_filter(spec, *filters->box_blur);
	} else if (strcmp(filter_type, "mg") == 0) {
		apply_filter(spec, *filters->med_gaus);
	} else {
		fprintf(stderr, "Error: Wrong filter type parameter '%s'\n", filter_type);
	}
}
