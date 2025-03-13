#include <stdio.h>
#include <stdlib.h>
#include "libbmp/libbmp.h"
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "main.h"

void init_filters(struct filter *blur, struct filter *motion_blur, struct filter *gaus_blur, struct filter *edges)
{
	motion_blur->size = 9;
	motion_blur->bias = 0.0;
	motion_blur->factor = 1.0 / 9.0;
	motion_blur->filter_arr = motion_blur_arr;

	blur->size = 5;
	blur->factor = 1.0 / 13.0;
	blur->bias = 0.0;
	blur->filter_arr = blur_arr;

	gaus_blur->size = 5;
	gaus_blur->bias = 0.0;
	gaus_blur->factor = 1.0 / 256.0;
	gaus_blur->filter_arr = gaus_blur_arr;

	edges->size = 5;
	edges->bias = 0.0;
	edges->factor = 1.0;
	edges->filter_arr = edges_arr;
}

// Function to compare two BMP images pixel-by-pixel
int compare_images(const bmp_img* img1, const bmp_img* img2)
{
	int width, height = 0;
    // Check if the dimensions of the images match
    if (img1->img_header.biWidth != img2->img_header.biWidth || img1->img_header.biHeight != img2->img_header.biHeight) {
        printf("Images have different dimensions!\n");
        return -1; // Dimensions are different
    }

    width = img1->img_header.biWidth;
    height = img1->img_header.biHeight;

    // Compare pixel values
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            bmp_pixel pixel1 = img1->img_pixels[y][x];
            bmp_pixel pixel2 = img2->img_pixels[y][x];

            // Compare RGB components of the pixels
            if (pixel1.red != pixel2.red || pixel1.green != pixel2.green || pixel1.blue != pixel2.blue) {
                printf("Difference found at pixel (%d, %d):\n", x, y);
                printf("Image 1 - R:%d G:%d B:%d\n", pixel1.red, pixel1.green, pixel1.blue);
                printf("Image 2 - R:%d G:%d B:%d\n", pixel2.red, pixel2.green, pixel2.blue);
                return 1; // Found a difference
            }
		}
    }
    return 0; // Images are identical
}

void apply_filter(bmp_img *input_img, bmp_img *output_img, int width, int height, struct filter cfilter)
{
    int x, y, filterX, filterY, imageX, imageY, weight = 0;
	bmp_pixel orig_pixel;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            int red = 0, green = 0, blue = 0;

            for (filterY = 0; filterY < cfilter.size; filterY++) {
                for (filterX = 0; filterX < cfilter.size; filterX++) {
                    imageX = (x + filterX - PADDING + width) % width;
                    imageY = (y + filterY - PADDING + height) % height;

                    // Check if the pixel is within bounds
                    if (imageX >= 0 && imageX < width && imageY >= 0 && imageY < height) {
                        orig_pixel = input_img->img_pixels[imageY][imageX];
                        weight = cfilter.filter_arr[filterY][filterX];

                        // Multiply the pixel value with the filter weight
                        red += orig_pixel.red * weight;
                        green += orig_pixel.green * weight;
                        blue += orig_pixel.blue * weight;
                    }
                }
            }

            // Normalize the values
			output_img->img_pixels[y][x].red = fmin(fmax((int)(red * cfilter.factor + cfilter.bias), 0), 255);
            output_img->img_pixels[y][x].green = fmin(fmax((int)(green * cfilter.factor + cfilter.bias), 0), 255);
            output_img->img_pixels[y][x].blue = fmin(fmax((int)(blue * cfilter.factor + cfilter.bias), 0), 255);
        }
    }
}

int main(int argc, char *argv[]) {
    bmp_img img, img_result;
    enum bmp_error status;
	char output_filepath[MAX_PATH_LEN];
	char input_filepath[MAX_PATH_LEN];
	struct filter blur, motion_blur, gaus_blur, edges; 

	if (argc < 3) {
        printf("Usage: %s <input_image> <filter_type>\n", argv[0]);
        return -1;
    }

    const char *input_filename = argv[1]; 
	const char *filter_type = argv[2];     

    printf("Input image: %s\n", input_filename);
    printf("Filter type: %s\n", filter_type);
	
    snprintf(input_filepath, sizeof(input_filepath), "test/%s", input_filename);

	// Read the input BMP image
    status = bmp_img_read(&img, input_filepath);
    if (status) {
        fprintf(stderr, "Error: Could not open BMP image\n");
        return -1;
    }

	int width = img.img_header.biWidth;
    int height = img.img_header.biHeight;

	bmp_img_init_df(&img_result, width, height);
	init_filters(&blur, &motion_blur, &gaus_blur, &edges);
	
	if (strcmp(filter_type, "mb") == 0) {
		apply_filter(&img, &img_result, width, height, motion_blur);
	} else if (strcmp(filter_type, "bb") == 0) {
		apply_filter(&img, &img_result, width, height, blur);
	} else if (strcmp(filter_type, "gb") == 0) {
		apply_filter(&img, &img_result, width, height, gaus_blur);
	} else if (strcmp(filter_type, "ed") == 0) {
		apply_filter(&img, &img_result, width, height, edges);
	} else {
		fprintf(stderr, "Wrong filter type parameter\n");
		return -1;
	}

    snprintf(output_filepath, sizeof(output_filepath), "test/output_%s", input_filename);

    // Save the filtered image
    bmp_img_write(&img_result, output_filepath);
	compare_images(&img, &img_result);
    
	bmp_img_free(&img);
    bmp_img_free(&img_result);

    printf("Processing complete. Filtered image saved as output.bmp\n");
    return 0;
}

