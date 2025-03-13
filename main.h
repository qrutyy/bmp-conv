#pragma once

#include <stdio.h>

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define MAX_PATH_LEN 16
#define MAX_FILTER_SIZE 9
#define PADDING (cfilter.size / 2)

struct filter{
	int size;
	double bias;
	double factor;
	double (*filter_arr)[MAX_FILTER_SIZE];
};

const double motion_blur_arr[9][9] = {
    {1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 1, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 1, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 1, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 1, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 1, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1}
};

const double blur_arr[5][5] =
{
	{0, 0, 1, 0, 0},
	{0, 1, 1, 1, 0},
	{1, 1, 1, 1, 1},
	{0, 1, 1, 1, 0},
	{0, 0, 1, 0, 0}
};

const double gaus_blur_arr[5][5] =
{
	{1,  4,  6,  4,  1},
	{4, 16, 24, 16,  4},
	{6, 24, 36, 24,  6},
	{4, 16, 24, 16,  4},
	{1,  4,  6,  4,  1}
};

const double edges_arr[5][5] =
{
	{0,  0, -1,  0,  0},
	{0,  0, -1,  0,  0},
	{0,  0,  2,  0,  0},
	{0,  0,  0,  0,  0},
	{0,  0,  0,  0,  0}
};
