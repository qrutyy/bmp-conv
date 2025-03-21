// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include "../libbmp/libbmp.h"
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "../utils/utils.h"

/** Comply to KISS, better* arch is shown in 2-con
  * * - however, not the best though, todo
  */

void apply_filter(bmp_img *input_img, bmp_img *output_img, int width, int height, struct filter cfilter)
{
	int x, y, filterX, filterY, imageX, imageY, weight = 0;
	bmp_pixel orig_pixel;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			int red = 0, green = 0, blue = 0;

			for (filterY = 0; filterY < cfilter.size; filterY++) {
				for (filterX = 0; filterX < cfilter.size; filterX++) {
					imageX = (x + filterX - PADDING + width) % width;
					imageY = (y + filterY - PADDING + height) % height;

					// Check if the pixel is within bounds
					if (imageX >= 0 && imageX < width && imageY >= 0 && imageY < height) {
						orig_pixel = input_img->img_pixels[imageY][imageX];
						weight = cfilter.filter_arr[filterY][filterX];

						// Multiply the pixel value with the filter weight
						red += orig_pixel.red * weight;
						green += orig_pixel.green * weight;
						blue += orig_pixel.blue * weight;
					}
				}
			}

			output_img->img_pixels[y][x].red =
				fmin(fmax((int)(red * cfilter.factor + cfilter.bias), 0), 255);
			output_img->img_pixels[y][x].green =
				fmin(fmax((int)(green * cfilter.factor + cfilter.bias), 0), 255);
			output_img->img_pixels[y][x].blue =
				fmin(fmax((int)(blue * cfilter.factor + cfilter.bias), 0), 255);
		}
	}
}

void apply_median_filter(bmp_img *input_img, bmp_img *output_img, int width, int height, int filter_size)
{
	int half_size = filter_size / 2;
	int filter_area = filter_size * filter_size;

	int *red = malloc(filter_area * sizeof(int));
	int *green = malloc(filter_area * sizeof(int));
	int *blue = malloc(filter_area * sizeof(int));

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int n = 0;

			// Collect neighboring pixels
			for (int filterY = -half_size; filterY <= half_size; filterY++) {
				for (int filterX = -half_size; filterX <= half_size; filterX++) {
					int imageX = (x + filterX + width) % width;
					int imageY = (y + filterY + height) % height;

					bmp_pixel orig_pixel = input_img->img_pixels[imageY][imageX];

					red[n] = orig_pixel.red;
					green[n] = orig_pixel.green;
					blue[n] = orig_pixel.blue;
					n++;
				}
			}

			// Apply median filter using selectKth to get the middle value
			output_img->img_pixels[y][x].red = selectKth(red, 0, filter_area, filter_area / 2);
			output_img->img_pixels[y][x].green = selectKth(green, 0, filter_area, filter_area / 2);
			output_img->img_pixels[y][x].blue = selectKth(blue, 0, filter_area, filter_area / 2);
		}
	}

	free(red);
	free(green);
	free(blue);
}

int main(int argc, char *argv[])
{
	bmp_img img, img_result;
	enum bmp_error status;
	char output_filepath[MAX_PATH_LEN];
	char input_filepath[MAX_PATH_LEN];
	const char *filter_type;
	const char *input_filename;
	struct filter blur, motion_blur, gaus_blur, conv, sharpen, embos;
	int width, height = 0;

	if (argc < 3) {
		printf("Usage: %s <input_image> <filter_type>\n", argv[0]);
		return -1;
	}

	input_filename = argv[1];
	filter_type = argv[2];

	printf("Input image: %s\n", input_filename);
	printf("Filter type: %s\n", filter_type);

	snprintf(input_filepath, sizeof(input_filepath), "../test/%s", input_filename);

	status = bmp_img_read(&img, input_filepath);
	if (status) {
		fprintf(stderr, "Error: Could not open BMP image\n");
		return -1;
	}

	width = img.img_header.biWidth;
	height = img.img_header.biHeight;

	bmp_img_init_df(&img_result, width, height);
	init_filters(&blur, &motion_blur, &gaus_blur, &conv, &sharpen, &embos);

	if (strcmp(filter_type, "mb") == 0) {
		apply_filter(&img, &img_result, width, height, motion_blur);
	} else if (strcmp(filter_type, "bb") == 0) {
		apply_filter(&img, &img_result, width, height, blur);
	} else if (strcmp(filter_type, "gb") == 0) {
		apply_filter(&img, &img_result, width, height, gaus_blur);
	} else if (strcmp(filter_type, "co") == 0) {
		apply_filter(&img, &img_result, width, height, conv);
	} else if (strcmp(filter_type, "sh") == 0) {
		apply_filter(&img, &img_result, width, height, sharpen);
	} else if (strcmp(filter_type, "em") == 0) {
		apply_filter(&img, &img_result, width, height, embos);
	} else if (strcmp(filter_type, "mm") == 0) {
		apply_median_filter(&img, &img_result, width, height, 15);
	} else {
		fprintf(stderr, "Wrong filter type parameter\n");
		return -1;
	}

	snprintf(output_filepath, sizeof(output_filepath), "../test/seq_out_%s", input_filename);

	bmp_img_write(&img_result, output_filepath);
	compare_images(&img, &img_result);

	bmp_img_free(&img);
	bmp_img_free(&img_result);

	printf("Processing complete. Filtered image saved as output.bmp\n");
	return 0;
}
