// SPDX-License-Identifier: GPL-3.0-or-later

#include "mt-utils.h"
#include "utils.h"
#include <pthread.h>
#include <stdint.h>
#include <string.h>

uint8_t process_by_row(struct thread_spec *th_spec, uint16_t *next_x_block, uint16_t block_size, pthread_mutex_t *x_block_mutex)
{
	pthread_mutex_lock(x_block_mutex);
	printf("next_block: %u, height: %d\n", *next_x_block,
	th_spec->dim->height);

	if (*next_x_block >= th_spec->dim->height) {
		pthread_mutex_unlock(x_block_mutex);
		th_spec->start_row = th_spec->end_row = 0;
		return 1;
	}
	th_spec->start_row = *next_x_block;
	*next_x_block += block_size;
	pthread_mutex_unlock(x_block_mutex);

	th_spec->start_column = 0;
	th_spec->end_row = fmin(th_spec->start_row + block_size, th_spec->dim->height);
	th_spec->end_column = th_spec->dim->width;

	return 0;
}

uint8_t process_by_column(struct thread_spec *th_spec, uint16_t *next_y_block, uint16_t block_size, pthread_mutex_t *y_block_mutex)
{
	pthread_mutex_lock(y_block_mutex);
	//	printf("next_block: %d, width: %d\n", *next_y_block,
	// th_spec->dim->width);

	if (*next_y_block >= th_spec->dim->width) {
		pthread_mutex_unlock(y_block_mutex);
		th_spec->start_column = th_spec->end_column = 0;
		return 1;
	}

	th_spec->start_column = *next_y_block;
	*next_y_block += block_size;
	pthread_mutex_unlock(y_block_mutex);

	th_spec->start_row = 0;
	th_spec->end_row = th_spec->dim->height;
	th_spec->end_column = fmin(th_spec->start_column + block_size, th_spec->dim->width);

	return 0;
}

uint8_t process_by_grid(struct thread_spec *th_spec, uint16_t *next_x_block, uint16_t *next_y_block, uint16_t block_size, pthread_mutex_t *xy_block_mutex)
{
	pthread_mutex_lock(xy_block_mutex);
	if (*next_x_block >= th_spec->dim->height || *next_y_block >= th_spec->dim->width) {
		pthread_mutex_unlock(xy_block_mutex);
		th_spec->start_row = th_spec->end_row = 0;
		th_spec->start_column = th_spec->end_column = 0;
		return 1;
	}

	th_spec->start_row = *next_x_block;
	th_spec->start_column = *next_y_block;
	*next_y_block += block_size;

	if (*next_y_block >= th_spec->dim->width) {
		*next_y_block = 0;
		*next_x_block += block_size;
	}
	pthread_mutex_unlock(xy_block_mutex);

	th_spec->end_row = fmin(th_spec->start_row + block_size, th_spec->dim->height);
	th_spec->end_column = fmin(th_spec->start_column + block_size, th_spec->dim->width);
	// printf("Row: st: %d, end: %d, Column: st: %d, end: %d \n",
	// th_spec->start_row, th_spec->end_row, th_spec->start_column,
	// th_spec->end_column);

	return 0;
}

uint8_t process_by_pixel(struct thread_spec *th_spec, uint16_t *next_x_block, uint16_t *next_y_block, pthread_mutex_t *xy_block_mutex)
{
	return process_by_grid(th_spec, next_x_block, next_y_block, 1, xy_block_mutex);
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

void filter_part_computation(struct thread_spec *spec, char* filter_type, struct filter_mix *filters)
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

// if you are confused -> check header file
struct img_dim *init_dimensions(uint16_t width, uint16_t height)
{
	struct img_dim *dim = malloc(sizeof(struct img_dim));
	if (!dim)
		return NULL;
	dim->width = width;
	dim->height = height;
	//	printf("Width: %d, Height: %d\n", width, height);
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

void *thread_spec_init(void)
{
	struct thread_spec *th_spec = malloc(sizeof(struct thread_spec));
	if (!th_spec)
		return NULL;

	return th_spec;
}
