#include <stdlib.h>
#include "../../libbmp/libbmp.h"
#include "utils.h"
#include "args-parse.h"
#include "filters.h"

// thread-specific parameters for computation only.
struct thread_spec {
	struct img_spec *img;
	struct img_dim *dim;
	struct sthreads_gen_info *st_gen_info;
	uint16_t start_column;
	uint16_t start_row;
	uint16_t end_row;
	uint16_t end_column;
};

// i know that isn't necessary, just a way to make it cleaner
// (somewhere without 6 '->'), just an alias
struct img_dim {
	uint16_t height;
	uint16_t width;
};

// since thread manages only 1 image computation -> this struct is used.
struct img_spec {
	bmp_img *input_img;
	bmp_img *output_img;
};

struct img_dim *init_dimensions(uint16_t width, uint16_t height);
struct img_spec *init_img_spec(bmp_img *input, bmp_img *output);
void apply_filter(struct thread_spec *spec, struct filter cfilter);
void apply_median_filter(struct thread_spec *spec, uint16_t filter_size);
void filter_part_computation(struct thread_spec *spec, char *filter_type, struct filter_mix *filters);
void *init_thread_spec(struct p_args *args, struct filter_mix *filters);
