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

void init_filters(struct filter_mix *filters);
void free_filters(struct filter_mix *filters);
