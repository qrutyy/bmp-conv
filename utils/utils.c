// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils.h"
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>

const double motion_blur_arr[9][9] = { { 1, 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 1, 0, 0, 0, 0, 0, 0, 0 },
				       { 0, 0, 1, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 1, 0, 0, 0, 0, 0 },
				       { 0, 0, 0, 0, 1, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 1, 0, 0, 0 },
				       { 0, 0, 0, 0, 0, 0, 1, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 1, 0 },
				       { 0, 0, 0, 0, 0, 0, 0, 0, 1 } };

const double blur_arr[5][5] = { { 0, 0, 1, 0, 0 },
				{ 0, 1, 1, 1, 0 },
				{ 1, 1, 1, 1, 1 },
				{ 0, 1, 1, 1, 0 },
				{ 0, 0, 1, 0, 0 } };

const double gaus_blur_arr[5][5] = { { 1, 4, 6, 4, 1 },
				     { 4, 16, 24, 16, 4 },
				     { 6, 24, 36, 24, 6 },
				     { 4, 16, 24, 16, 4 },
				     { 1, 4, 6, 4, 1 } };

const double conv_arr[3][3] = { { 0, 0, 0 }, { 0, 1, 0 }, { 0, 0, 0 } };

const double sharpen_arr[3][3] = { { -1, -1, -1 }, { -1, 9, -1 }, { -1, -1, -1 } };

const double emboss_arr[5][5] = { { -1, -1, -1, -1, 0 },
				  { -1, -1, -1, 0, 1 },
				  { -1, -1, 0, 1, 1 },
				  { -1, 0, 1, 1, 1 },
				  { 0, 1, 1, 1, 1 } };

const double big_gaus_arr[15][15] = {
    {2, 2, 3, 3, 4, 4, 5, 5, 5, 4, 4, 3, 3, 2, 2},
    {2, 3, 3, 4, 4, 5, 5, 6, 5, 5, 4, 4, 3, 3, 2},
    {3, 3, 4, 5, 5, 6, 6, 7, 6, 6, 5, 5, 4, 3, 3},
    {3, 4, 5, 6, 7, 7, 8, 8, 8, 7, 7, 6, 5, 4, 3},
    {4, 4, 5, 7, 8, 9, 9, 10, 9, 9, 8, 7, 5, 4, 4},
    {4, 5, 6, 7, 9, 10, 11, 11, 11, 10, 9, 7, 6, 5, 4},
    {5, 5, 6, 8, 9, 11, 12, 12, 12, 11, 9, 8, 6, 5, 5},
    {5, 6, 7, 8, 10, 11, 12, 13, 12, 11, 10, 8, 7, 6, 5},
    {5, 5, 6, 8, 9, 11, 12, 12, 12, 11, 9, 8, 6, 5, 5},
    {4, 5, 6, 7, 9, 10, 11, 11, 11, 10, 9, 7, 6, 5, 4},
    {4, 4, 5, 7, 8, 9, 9, 10, 9, 9, 8, 7, 5, 4, 4},
    {3, 4, 5, 6, 7, 7, 8, 8, 8, 7, 7, 6, 5, 4, 3},
    {3, 3, 4, 5, 5, 6, 6, 7, 6, 6, 5, 5, 4, 3, 3},
    {2, 3, 3, 4, 4, 5, 5, 6, 5, 5, 4, 4, 3, 3, 2},
    {2, 2, 3, 3, 4, 4, 5, 5, 5, 4, 4, 3, 3, 2, 2}
};
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

double get_time_in_seconds(void)
{
	struct timeval time;
	gettimeofday(&time, NULL);
	return (double)time.tv_sec + (double)time.tv_usec * 0.000001;
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
				return 1;
			}
		}
	}
	return 0;
}

static void init_filter(struct filter *f, int size, double bias, double factor, const double arr[size][size])
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

static void free_filter(struct filter *f)
{
	for (int i = 0; i < f->size; i++) {
		free(f->filter_arr[i]);
	}
	free(f->filter_arr);
}

void init_filters(struct filter_mix *filters)
{
	init_filter(filters->motion_blur, 9, 0.0, 1.0 / 9.0, motion_blur_arr);
	init_filter(filters->blur, 5, 0.0, 1.0 / 13.0, blur_arr);
	init_filter(filters->gaus_blur, 5, 0.0, 1.0 / 256.0, gaus_blur_arr);
	init_filter(filters->conv, 3, 0.0, 1.0, conv_arr);
	init_filter(filters->sharpen, 3, 0.0, 1.0, sharpen_arr);
	init_filter(filters->emboss, 5, 128.0, 1.0, emboss_arr);
	init_filter(filters->big_gaus, 15, 0.0, 1.0 / 771, big_gaus_arr);
}

void free_filters(struct filter_mix *filters)
{
	free_filter(filters->motion_blur);
	free_filter(filters->blur);
	free_filter(filters->gaus_blur);
	free_filter(filters->conv);
	free_filter(filters->sharpen);
	free_filter(filters->emboss);
	free_filter(filters->big_gaus);
}
