// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdio.h>
#include "../libbmp/libbmp.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define MAX_PATH_LEN 30
#define MAX_FILTER_SIZE 9
#define PADDING (cfilter.size / 2)

struct filter {
	int size;
	double bias;
	double factor;
	double **filter_arr;
};

void swap(int *a, int *b);
int selectKth(int *data, int s, int e, int k);
double get_time_in_seconds(void);
int compare_images(const bmp_img *img1, const bmp_img *img2);
void free_filters(struct filter *blur, struct filter *motion_blur, struct filter *gaus_blur, struct filter *conv,
		  struct filter *sharpen, struct filter *emboss);
void init_filters(struct filter *blur, struct filter *motion_blur, struct filter *gaus_blur, struct filter *conv,
		  struct filter *sharpen, struct filter *emboss);
