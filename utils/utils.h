#pragma once

#include <stdio.h>
#include "../libbmp/libbmp.h"

void swap(int *a, int *b);
int selectKth(int *data, int s, int e, int k);
double get_time_in_seconds(void);
int compare_images(const bmp_img *img1, const bmp_img *img2);
