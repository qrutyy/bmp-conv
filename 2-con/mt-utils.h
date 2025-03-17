// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdio.h>
#include "../libbmp/libbmp.h"
#include "../utils/utils.h"
#include <math.h>
#include <stdlib.h>

enum compute_mode { BY_ROW, BY_COLUMN, BY_PIXEL, BY_GRID };

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

void apply_filter(struct thread_spec *spec, struct filter cfilter);
void apply_median_filter(struct thread_spec *spec, int filter_size);
struct img_dim *init_dimensions(int width, int height);
struct img_spec *init_img_spec(bmp_img *input, bmp_img *output);
