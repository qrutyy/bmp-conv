#include <stdio.h>
#include <stdlib.h>
#include "../libbmp/libbmp.h"
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "main.h"
#include <pthread.h>
#include <sys/time.h>
#include "../utils/utils.h"

/** TODO
  * make thread mode (columns/rows per thread == const)
  */

const char *input_filename;
const char *filter_type;
struct filter blur, motion_blur, gaus_blur, conv, sharpen, embos;
pthread_mutex_t input_img_m = PTHREAD_MUTEX_INITIALIZER;

void init_filter(struct filter *f, int size, double bias, double factor, const double arr[size][size])
{
	f->size = size;
	f->bias = bias;
	f->factor = factor;

	f->filter_arr = malloc(size * sizeof(double *));
	for (int i = 0; i < size; i++) {
		f->filter_arr[i] = malloc(size * sizeof(double));
		memcpy(f->filter_arr[i], arr[i], size * sizeof(double));
	}
}

void free_filter(struct filter *f)
{
	for (int i = 0; i < f->size; i++) {
		free(f->filter_arr[i]);
	}
	free(f->filter_arr);
}

void init_filters(struct filter *blur, struct filter *motion_blur, struct filter *gaus_blur, struct filter *conv,
		  struct filter *sharpen, struct filter *emboss)
{
	init_filter(motion_blur, 9, 0.0, 1.0 / 9.0, motion_blur_arr);
	init_filter(blur, 5, 0.0, 1.0 / 13.0, blur_arr);
	init_filter(gaus_blur, 5, 0.0, 1.0 / 256.0, gaus_blur_arr);
	init_filter(conv, 3, 0.0, 1.0, conv_arr);
	init_filter(sharpen, 3, 0.0, 1.0, sharpen_arr);
	init_filter(emboss, 5, 128.0, 1.0, emboss_arr);
}

void free_filters(struct filter *blur, struct filter *motion_blur, struct filter *gaus_blur, struct filter *conv,
		  struct filter *sharpen, struct filter *emboss)
{
	free_filter(motion_blur);
	free_filter(blur);
	free_filter(gaus_blur);
	free_filter(conv);
	free_filter(sharpen);
	free_filter(emboss);
}

// if you are confused -> check header file
struct img_dim *init_dimensions(int width, int height)
{
	struct img_dim *dim = malloc(sizeof(struct img_dim));
	if (!dim)
		return NULL;
	dim->width = width;
	dim->height = height;
	return dim;
}

struct img_spec *init_img_spec(bmp_img *input, bmp_img *output)
{
	struct img_spec *spec = malloc(sizeof(struct img_spec));
	if (!spec)
		return NULL;
	spec->input_img = input;
	spec->output_img = output;
	return spec;
}

void apply_filter(struct thread_spec *spec, struct filter cfilter)
{
	int x, y, filterX, filterY, imageX, imageY, weight = 0;
	bmp_pixel orig_pixel;
	printf("start x:%d y:%d, end x:%d y:%d\n", spec->start_column, spec->start_row, spec->end_column, spec->end_row);
	
	for (x = spec->start_column; x < spec->end_column; x++) {
		for (y = spec->start_row; y < spec->end_row; y++) {
			int red = 0, green = 0, blue = 0;

			for (filterY = 0; filterY < cfilter.size; filterY++) {
				for (filterX = 0; filterX < cfilter.size; filterX++) {
					imageX = (x + filterX - PADDING + spec->dim->width) % spec->dim->width;
					imageY = (y + filterY - PADDING + spec->dim->height) % spec->dim->height;

					// Check if the pixel is within bounds
					if (imageX >= 0 && imageX < spec->dim->width && imageY >= 0 &&
					    imageY < spec->dim->height) {
						orig_pixel = spec->img->input_img->img_pixels[imageY][imageX];
						weight = cfilter.filter_arr[filterY][filterX];

						// Multiply the pixel value with the filter weight
						red += orig_pixel.red * weight;
						green += orig_pixel.green * weight;
						blue += orig_pixel.blue * weight;
					}
				}
			}

			spec->img->output_img->img_pixels[y][x].red =
				fmin(fmax((int)(red * cfilter.factor + cfilter.bias), 0), 255);
			spec->img->output_img->img_pixels[y][x].green =
				fmin(fmax((int)(green * cfilter.factor + cfilter.bias), 0), 255);
			spec->img->output_img->img_pixels[y][x].blue =
				fmin(fmax((int)(blue * cfilter.factor + cfilter.bias), 0), 255);
		}
	}
}

void apply_median_filter(struct thread_spec *spec, int filter_size)
{
	int half_size = filter_size / 2;
	int filter_area = filter_size * filter_size;

	int *red = malloc(filter_area * sizeof(int));
	int *green = malloc(filter_area * sizeof(int));
	int *blue = malloc(filter_area * sizeof(int));

	for (int x = spec->start_column; x < spec->end_column; x++) {
		for (int y = spec->start_row; y < spec->end_row; y++) {
			int n = 0;

			for (int filterY = -half_size; filterY <= half_size; filterY++) {
				for (int filterX = -half_size; filterX <= half_size; filterX++) {
					int imageX = (x + filterX + spec->dim->width) % spec->dim->width;
					int imageY = (y + filterY + spec->dim->height) % spec->dim->height;

					bmp_pixel orig_pixel = spec->img->input_img->img_pixels[imageY][imageX];

					red[n] = orig_pixel.red;
					green[n] = orig_pixel.green;
					blue[n] = orig_pixel.blue;
					n++;
				}
			}

			// Apply median filter using selectKth to get the middle value
			spec->img->output_img->img_pixels[y][x].red = selectKth(red, 0, filter_area, filter_area / 2);
			spec->img->output_img->img_pixels[y][x].green =
				selectKth(green, 0, filter_area, filter_area / 2);
			spec->img->output_img->img_pixels[y][x].blue = selectKth(blue, 0, filter_area, filter_area / 2);
		}
	}

	free(red);
	free(green);
	free(blue);
}

int parse_args(int argc, char *argv[], int *threadnum, enum compute_mode *mode)
{
	const char *mode_str; 

	if (argc < 4) {
		printf("Usage: %s <input_image> <filter_type>\n", argv[0]);
		return -1;
	}

	input_filename = argv[1];
	filter_type = argv[2];

	printf("Input image: %s\n", input_filename);
	printf("Filter type: %s\n", filter_type);

	if (strncmp(argv[3], "--threadnum=", 12) == 0) {
		*threadnum = atoi(argv[3] + 12);
		printf("Number of threads: %d\n", *threadnum);
	} else {
		fprintf(stderr, "Please use correct arg descriptors\n");
		return -1;
	}
	
	if (strncmp(argv[4], "--mode=", 7) == 0) {
        mode_str = argv[4] + 7;

        if (strcmp(mode_str, "by_row") == 0) {
            *mode = BY_ROW;
        } else if (strcmp(mode_str, "by_column") == 0) {
            *mode = BY_COLUMN;
        } else if (strcmp(mode_str, "by_pixel") == 0) {
            *mode = BY_PIXEL;
        } else if (strcmp(mode_str, "by_grid") == 0) {
            *mode = BY_GRID;
        } else {
            fprintf(stderr, "Error: Invalid mode. Use by_row, by_column, by_pixel, or by_grid\n");
            return -1;
        }
        printf("Mode selected: %s\n\n", mode_str);
    } else {
        fprintf(stderr, "Error: Missing or incorrect --mode argument\n");
        return -1;
    }
	
	return 0;
}

void filter_part_computation(struct thread_spec *spec)
{
	if (strcmp(filter_type, "mb") == 0) {
		apply_filter(spec, motion_blur);
	} else if (strcmp(filter_type, "bb") == 0) {
		apply_filter(spec, blur);
	} else if (strcmp(filter_type, "gb") == 0) {
		apply_filter(spec, gaus_blur);
	} else if (strcmp(filter_type, "co") == 0) {
		apply_filter(spec, conv);
	} else if (strcmp(filter_type, "sh") == 0) {
		apply_filter(spec, sharpen);
	} else if (strcmp(filter_type, "em") == 0) {
		apply_filter(spec, embos);
	} else if (strcmp(filter_type, "mm") == 0) {
		apply_median_filter(spec, 15);
	} else {
		fprintf(stderr, "Wrong filter type parameter\n");
	}
}

void *thread_function(void *arg)
{
	struct thread_spec *spec = (struct thread_spec *)arg;
	filter_part_computation(spec);
	free(spec);
	return NULL;
}

void *thread_spec_init(bmp_img *img, int i, int threadnum, enum compute_mode m)
{
	int count = 0;
	struct thread_spec *th_spec = malloc(sizeof(struct thread_spec));
	if (!th_spec)
		return NULL;

	switch (m) {
		case BY_ROW:
			count = img->img_header.biHeight / threadnum;
			th_spec->start_row = i * count;
			th_spec->start_column = 0;
	  		th_spec->end_row = th_spec->start_row + count;
			th_spec->end_column = img->img_header.biWidth;
			break;
		case BY_COLUMN:
			count = img->img_header.biWidth / threadnum;
			th_spec->start_column = i * count;
			th_spec->start_row = 0;
			th_spec->end_row = img->img_header.biHeight;
			th_spec->end_column = th_spec->start_column + count;
			break;
		case BY_PIXEL:
			fprintf(stderr, "Error: Not implemented yet\n");
			return NULL;
		case BY_GRID:
			fprintf(stderr, "Error: Not implemented yet\n");
			return NULL;
	}
	return th_spec;
}

int main(int argc, char *argv[])
{
	bmp_img img, img_result;
	pthread_t *th = NULL;
	struct img_dim *dim = NULL;
	struct img_spec *img_spec = NULL;
	struct thread_spec *th_spec = NULL;
	char input_filepath[MAX_PATH_LEN], output_filepath[MAX_PATH_LEN];
	double start_time, end_time = 0;
	int i = 0, threadnum;
	enum compute_mode mode;

	parse_args(argc, argv, &threadnum, &mode);
	th = malloc(threadnum * sizeof(pthread_t));
	if (!th)
		goto mem_err;

	snprintf(input_filepath, sizeof(input_filepath), "../test/%s", input_filename);

	if (bmp_img_read(&img, input_filepath)) {
		fprintf(stderr, "Error: Could not open BMP image\n");
		return -1;
	}
	if (img.img_header.biWidth % threadnum != 0) {
		fprintf(stderr, "Warning: threadnum should divide BMP width with no remainder\n");
		do {
			threadnum--;
		} while (img.img_header.biWidth % threadnum == 0);

		fprintf(stderr, "Warning: decreased threadnum to %d\n", threadnum);
		if (threadnum <= 1) {
			fprintf(stderr, "Error: no dividers of biWidth found. Try bigger value\n");
			return -1;
		}
	}

	dim = init_dimensions(img.img_header.biWidth, img.img_header.biHeight);
	if (!dim)
		goto mem_err;

	bmp_img_init_df(&img_result, dim->width, dim->height);
	img_spec = init_img_spec(&img, &img_result);
	init_filters(&blur, &motion_blur, &gaus_blur, &conv, &sharpen, &embos);

	start_time = get_time_in_seconds();

	for (i = 0; i < threadnum; i++) {
		th_spec = thread_spec_init(&img, i, threadnum, mode);
		if (!th_spec){
			goto mem_err;
		}
		th_spec->dim = dim;
		th_spec->img = img_spec;

		if (pthread_create(&th[i], NULL, thread_function, th_spec) != 0) {
			perror("Failed to create a thread");
			free(th_spec);
		}
	}

	for (i = 0; i < threadnum; i++)
		pthread_join(th[i], NULL);

	end_time = get_time_in_seconds();

	printf("RESULT: filter = %s, threadnum = %d, time = %.6f seconds\n", filter_type, threadnum,
	       end_time - start_time);
	snprintf(output_filepath, sizeof(output_filepath), "../test/con_out_%s", input_filename);

	bmp_img_write(&img_result, output_filepath);
	compare_images(&img, &img_result);

	free(th);
	free(dim);
	bmp_img_free(&img);
	bmp_img_free(&img_result);
	return 0;

mem_err:
	fprintf(stderr, "Memory allocation error\n");
	free(th);
	free(dim);
	return -1;
}
