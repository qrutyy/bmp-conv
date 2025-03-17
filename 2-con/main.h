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

enum compute_mode {
	BY_ROW,
	BY_COLUMN,
	BY_PIXEL,
	BY_GRID
};

// i know that isn't necessary, just a way to make it cleaner (without 6 -> somewhere)
struct img_dim {
	int height;
	int width;
};

struct img_spec {
	bmp_img *input_img;
	bmp_img *output_img;
};

struct thread_spec {
	struct img_spec *img;
	struct img_dim *dim;
	int start_column;
	int start_row;
	int end_row;
	int end_column;
};

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
