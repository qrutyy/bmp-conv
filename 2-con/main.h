#pragma once

#include <stdio.h>

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define MAX_PATH_LEN 24
#define MAX_FILTER_SIZE 9
#define PADDING (cfilter->size / 2)

struct filter {
	int size;
	double bias;
	double factor;
	double **filter_arr;
};

struct thread_spec {
	int start_column;
	int width;
	int height;
	int count;
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
