// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

struct filter {
	int size;
	double bias;
	double factor;
	double **filter_arr;
};

struct filter_mix {
	struct filter *blur;
	struct filter *motion_blur;
	struct filter *gaus_blur;
	struct filter *conv;
	struct filter *sharpen;
	struct filter *emboss;
	struct filter *big_gaus;
	struct filter *med_gaus;
	struct filter *box_blur;
};

/**
 * Initializes all predefined filter types within the filter_mix structure by calling init_filter for each one with its corresponding kernel matrix and parameters.
 *
 * @param filters Pointer to the filter_mix structure to be initialized. Assumes the structure itself is already allocated.
 */
void init_filters(struct filter_mix *filters);

/**
 * Frees the memory associated with all predefined filter types stored within the filter_mix structure by calling free_filter for each one.
 *
 * @param filters Pointer to the filter_mix structure whose filters need freeing.
 */
void free_filters(struct filter_mix *filters);
