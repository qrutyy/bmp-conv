// SPDX-License-Identifier: GPL-3.0-or-later

#include "mt-utils.h"
#include "../utils/utils.h"

void apply_filter(struct thread_spec *spec, struct filter cfilter)
{
	int x, y, filterX, filterY, imageX, imageY, weight = 0;
	bmp_pixel orig_pixel;
	printf("start x:%d y:%d, end x:%d y:%d\n", spec->start_column, spec->start_row, spec->end_column,
	       spec->end_row);

	for (x = spec->start_column; x < spec->end_column; x++) {
		for (y = spec->start_row; y < spec->end_row; y++) {
			int red = 0, green = 0, blue = 0;

			for (filterY = 0; filterY < cfilter.size; filterY++) {
				for (filterX = 0; filterX < cfilter.size; filterX++) {
					imageX = (x + filterX - PADDING + spec->dim->width) % spec->dim->width;
					imageY = (y + filterY - PADDING + spec->dim->height) % spec->dim->height;

					// Check if the pixel is within bounds
					if (imageX >= 0 && imageX < spec->dim->width && imageY >= 0 &&
					    imageY < spec->dim->height) {
						orig_pixel = spec->img->input_img->img_pixels[imageY][imageX];
						weight = cfilter.filter_arr[filterY][filterX];

						// Multiply the pixel value with the filter weight
						red += orig_pixel.red * weight;
						green += orig_pixel.green * weight;
						blue += orig_pixel.blue * weight;
					}
				}
			}

			spec->img->output_img->img_pixels[y][x].red =
				fmin(fmax((int)(red * cfilter.factor + cfilter.bias), 0), 255);
			spec->img->output_img->img_pixels[y][x].green =
				fmin(fmax((int)(green * cfilter.factor + cfilter.bias), 0), 255);
			spec->img->output_img->img_pixels[y][x].blue =
				fmin(fmax((int)(blue * cfilter.factor + cfilter.bias), 0), 255);
		}
	}
}

void apply_median_filter(struct thread_spec *spec, int filter_size)
{
	int half_size = filter_size / 2;
	int filter_area = filter_size * filter_size;

	int *red = malloc(filter_area * sizeof(int));
	int *green = malloc(filter_area * sizeof(int));
	int *blue = malloc(filter_area * sizeof(int));

	for (int x = spec->start_column; x < spec->end_column; x++) {
		for (int y = spec->start_row; y < spec->end_row; y++) {
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
			spec->img->output_img->img_pixels[y][x].green =
				selectKth(green, 0, filter_area, filter_area / 2);
			spec->img->output_img->img_pixels[y][x].blue = selectKth(blue, 0, filter_area, filter_area / 2);
		}
	}

	free(red);
	free(green);
	free(blue);
}

// if you are confused -> check header file
struct img_dim *init_dimensions(int width, int height)
{
	struct img_dim *dim = malloc(sizeof(struct img_dim));
	if (!dim)
		return NULL;
	dim->width = width;
	dim->height = height;
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
