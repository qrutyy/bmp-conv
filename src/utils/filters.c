// SPDX-License-Identifier: GPL-3.0-or-later

#include "filters.h"
#include "../../logger/log.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

const double motion_blur_arr[9][9] = { { 1, 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 1, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0, 1, 0, 0, 0, 0, 0, 0 },
				       { 0, 0, 0, 1, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 1, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 1, 0, 0, 0 },
				       { 0, 0, 0, 0, 0, 0, 1, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 1, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0, 1 } };

const double blur_arr[5][5] = { { 0, 0, 1, 0, 0 }, { 0, 1, 1, 1, 0 }, { 1, 1, 1, 1, 1 }, { 0, 1, 1, 1, 0 }, { 0, 0, 1, 0, 0 } };

const double gaus_blur_arr[5][5] = { { 1, 4, 6, 4, 1 }, { 4, 16, 24, 16, 4 }, { 6, 24, 36, 24, 6 }, { 4, 16, 24, 16, 4 }, { 1, 4, 6, 4, 1 } };

const double conv_arr[3][3] = { { 0, 0, 0 }, { 0, 1, 0 }, { 0, 0, 0 } }; // Identity kernel

const double sharpen_arr[3][3] = { { -1, -1, -1 }, { -1, 9, -1 }, { -1, -1, -1 } };

const double emboss_arr[5][5] = { { -1, -1, -1, -1, 0 }, { -1, -1, -1, 0, 1 }, { -1, -1, 0, 1, 1 }, { -1, 0, 1, 1, 1 }, { 0, 1, 1, 1, 1 } };

const double big_gaus_arr[15][15] = {
	{ 2, 2, 3, 3, 4, 4, 5, 5, 5, 4, 4, 3, 3, 2, 2 },      { 2, 3, 3, 4, 4, 5, 5, 6, 5, 5, 4, 4, 3, 3, 2 },	      { 3, 3, 4, 5, 5, 6, 6, 7, 6, 6, 5, 5, 4, 3, 3 },
	{ 3, 4, 5, 6, 7, 7, 8, 8, 8, 7, 7, 6, 5, 4, 3 },      { 4, 4, 5, 7, 8, 9, 9, 10, 9, 9, 8, 7, 5, 4, 4 },	      { 4, 5, 6, 7, 9, 10, 11, 11, 11, 10, 9, 7, 6, 5, 4 },
	{ 5, 5, 6, 8, 9, 11, 12, 12, 12, 11, 9, 8, 6, 5, 5 }, { 5, 6, 7, 8, 10, 11, 12, 13, 12, 11, 10, 8, 7, 6, 5 }, { 5, 5, 6, 8, 9, 11, 12, 12, 12, 11, 9, 8, 6, 5, 5 },
	{ 4, 5, 6, 7, 9, 10, 11, 11, 11, 10, 9, 7, 6, 5, 4 }, { 4, 4, 5, 7, 8, 9, 9, 10, 9, 9, 8, 7, 5, 4, 4 },	      { 3, 4, 5, 6, 7, 7, 8, 8, 8, 7, 7, 6, 5, 4, 3 },
	{ 3, 3, 4, 5, 5, 6, 6, 7, 6, 6, 5, 5, 4, 3, 3 },      { 2, 3, 3, 4, 4, 5, 5, 6, 5, 5, 4, 4, 3, 3, 2 },	      { 2, 2, 3, 3, 4, 4, 5, 5, 5, 4, 4, 3, 3, 2, 2 }
};

const double med_gaus_arr[9][9] = { { 1, 1, 2, 2, 2, 2, 2, 1, 1 }, { 1, 2, 2, 3, 3, 3, 2, 2, 1 }, { 2, 2, 3, 4, 5, 4, 3, 2, 2 },
				    { 2, 3, 4, 5, 6, 5, 4, 3, 2 }, { 2, 3, 5, 6, 7, 6, 5, 3, 2 }, { 2, 3, 4, 5, 6, 5, 4, 3, 2 },
				    { 2, 2, 3, 4, 5, 4, 3, 2, 2 }, { 1, 2, 2, 3, 3, 3, 2, 2, 1 }, { 1, 1, 2, 2, 2, 2, 2, 1, 1 } };

const double box_blur_arr[15][15] = {
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }
};

/**
 * Allocates memory for a filter structure and its associated kernel matrix, then copies the provided kernel data, bias, and factor into the structure. Exits fatally if memory allocation fails.
 *
 * @param f Pointer to a filter pointer (`struct filter **`) which will be updated to point to the newly allocated and initialized filter structure.
 * @param size The dimension (width and height) of the square filter kernel matrix.
 * @param bias A value added to the result of the convolution operation.
 * @param factor A scaling factor applied to the result of the convolution operation.
 * @param arr A 2D array of doubles representing the filter kernel matrix. Must have dimensions `size` x `size`.
 */
static void init_filter(struct filter **f, int size, double bias, double factor, const double arr[size][size])
{
	*f = malloc(sizeof(struct filter));
	if (!*f) {
		log_error("Memory allocation failed for filter struct\n");
		exit(1);
	}
	(*f)->size = size;
	(*f)->bias = bias;
	(*f)->factor = factor;

	(*f)->filter_arr = malloc(size * sizeof(double *));
	if (!(*f)->filter_arr) {
		log_error("Memory allocation failed for filter_arr rows\n");
		free(*f);
		exit(1);
	}
	for (int i = 0; i < size; i++) {
		(*f)->filter_arr[i] = malloc(size * sizeof(double));
		if (!(*f)->filter_arr[i]) {
			log_error("Memory allocation failed for filter_arr column %d\n", i);
			// Cleanup previously allocated rows and the main struct
			for (int j = 0; j < i; ++j)
				free((*f)->filter_arr[j]);
			free((*f)->filter_arr);
			free(*f);
			exit(1);
		}
		memcpy((*f)->filter_arr[i], arr[i], size * sizeof(double));
	}
}

/**
 * Frees the memory allocated for a filter structure, including its kernel matrix.
 *
 * @param f Pointer to the filter structure to be freed. Assumes `f` and its internal arrays were allocated by `init_filter` or equivalent.
 */
static void free_filter(struct filter *f)
{
	if (!f)
		return;
	if (f->filter_arr) {
		for (int i = 0; i < f->size; i++) {
			free(f->filter_arr[i]);
		}
		free(f->filter_arr);
	}
	free(f);
}

void init_filters(struct filter_mix *filters)
{
	init_filter(&filters->motion_blur, 9, 0.0, 1.0 / 9.0, motion_blur_arr);
	init_filter(&filters->blur, 5, 0.0, 1.0 / 13.0, blur_arr);
	init_filter(&filters->gaus_blur, 5, 0.0, 1.0 / 256.0, gaus_blur_arr);
	init_filter(&filters->conv, 3, 0.0, 1.0, conv_arr);
	init_filter(&filters->sharpen, 3, 0.0, 1.0, sharpen_arr);
	init_filter(&filters->emboss, 5, 128.0, 1.0, emboss_arr);
	init_filter(&filters->big_gaus, 15, 0.0, 1.0 / 771.0, big_gaus_arr);
	init_filter(&filters->med_gaus, 9, 0.0, 1.0 / 213.0, med_gaus_arr);
	init_filter(&filters->box_blur, 15, 0.0, 1.0 / 225.0, box_blur_arr); // 15x15 = 225
}

void free_filters(struct filter_mix *filters)
{
	if (!filters)
		return;
	free_filter(filters->motion_blur);
	free_filter(filters->blur);
	free_filter(filters->gaus_blur);
	free_filter(filters->conv);
	free_filter(filters->sharpen);
	free_filter(filters->emboss);
	free_filter(filters->big_gaus);
	free_filter(filters->med_gaus);
	free_filter(filters->box_blur);
	filters->motion_blur = NULL;
	filters->blur = NULL;
	filters->gaus_blur = NULL;
	filters->conv = NULL;
	filters->sharpen = NULL;
	filters->emboss = NULL;
	filters->big_gaus = NULL;
	filters->med_gaus = NULL;
	filters->box_blur = NULL;
}
