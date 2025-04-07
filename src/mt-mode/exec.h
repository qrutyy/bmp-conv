#include "../../libbmp/libbmp.h"
#include "../utils/threads-general.h"

#pragma once

double execute_mt_computation(int threadnum, struct img_dim *dim, struct img_spec *img_spec, struct p_args *args, struct filter_mix *filters); 
void sthreads_save(char *output_filepath, size_t path_len, int threadnum, bmp_img *img_result, struct p_args *args);


