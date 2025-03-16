#include <stdio.h>
#include <stdlib.h>
#include "../libbmp/libbmp.h"
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "main.h"

void init_filter(struct filter *f, int size, double bias, double factor, const double arr[size][size])
{
	f->size = size;
	f->bias = bias;
	f->factor = factor;

	f->filter_arr = malloc(size * sizeof(double *));
	for (int i = 0; i < size; i++) {
		f->filter_arr[i] = malloc(size * sizeof(double));
		memcpy(f->filter_arr[i], arr[i], size * sizeof(double));
	}
}

void free_filter(struct filter *f)
{
	for (int i = 0; i < f->size; i++) {
		free(f->filter_arr[i]);
	}
	free(f->filter_arr);
}

void init_filters(struct filter *blur, struct filter *motion_blur, struct filter *gaus_blur, struct filter *conv,
		  struct filter *sharpen, struct filter *emboss)
{
	init_filter(motion_blur, 9, 0.0, 1.0 / 9.0, motion_blur_arr);
	init_filter(blur, 5, 0.0, 1.0 / 13.0, blur_arr);
	init_filter(gaus_blur, 5, 0.0, 1.0 / 256.0, gaus_blur_arr);
	init_filter(conv, 3, 0.0, 1.0, conv_arr);
	init_filter(sharpen, 3, 0.0, 1.0, sharpen_arr);
	init_filter(emboss, 5, 128.0, 1.0, emboss_arr);
}

void free_filters(struct filter *blur, struct filter *motion_blur, struct filter *gaus_blur, struct filter *conv,
		  struct filter *sharpen, struct filter *emboss)
{
	free_filter(motion_blur);
	free_filter(blur);
	free_filter(gaus_blur);
	free_filter(conv);
	free_filter(sharpen);
	free_filter(emboss);
}

int compare_images(const bmp_img *img1, const bmp_img *img2)
{
	int width, height = 0;
	if (img1->img_header.biWidth != img2->img_header.biWidth ||
	    img1->img_header.biHeight != img2->img_header.biHeight) {
		printf("Images have different dimensions!\n");
		return -1;
	}

	width = img1->img_header.biWidth;
	height = img1->img_header.biHeight;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			bmp_pixel pixel1 = img1->img_pixels[y][x];
			bmp_pixel pixel2 = img2->img_pixels[y][x];

			if (pixel1.red != pixel2.red || pixel1.green != pixel2.green || pixel1.blue != pixel2.blue) {
				printf("Difference found at pixel (%d, %d):\n", x, y);
				printf("Image 1 - R:%d G:%d B:%d\n", pixel1.red, pixel1.green, pixel1.blue);
				printf("Image 2 - R:%d G:%d B:%d\n", pixel2.red, pixel2.green, pixel2.blue);
				return 1; // Found a difference
			}
		}
	}
	return 0; // Images are identical
}

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

void swap(int *a, int *b)
{
	int temp = *a;
	*a = *b;
	*b = temp;
}

int selectKth(int *data, int s, int e, int k)
{
	if (e - s <= 5) {
		for (int i = s + 1; i < e; i++)
			for (int j = i; j > s && data[j - 1] > data[j]; j--)
				swap(&data[j], &data[j - 1]);
		return data[s + k];
	}

	int p = (s + e) / 2;
	swap(&data[p], &data[e - 1]);

	int j = s;
	for (int i = s; i < e - 1; i++)
		if (data[i] < data[e - 1])
			swap(&data[i], &data[j++]);

	swap(&data[j], &data[e - 1]);

	if (k == j - s)
		return data[j];
	else if (k < j - s)
		return selectKth(data, s, j, k);
	else
		return selectKth(data, j + 1, e, k - (j - s + 1));
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

	snprintf(output_filepath, sizeof(output_filepath), "../test/output_%s", input_filename);

	bmp_img_write(&img_result, output_filepath);
	compare_images(&img, &img_result);

	bmp_img_free(&img);
	bmp_img_free(&img_result);

	printf("Processing complete. Filtered image saved as output.bmp\n");
	return 0;
}
