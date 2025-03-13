#include <stdio.h>
#include <stdlib.h>
#include "libbmp/libbmp.h"
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "main.h"

double factor = 1.0 / 9.0;
double bias = 0.0;

double motion_blur[9][9] = {
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

double blur[5][5] =
{
  0, 0, 1, 0, 0,
  0, 1, 1, 1, 0,
  1, 1, 1, 1, 1,
  0, 1, 1, 1, 0,
  0, 0, 1, 0, 0,
};



// Function to compare two BMP images pixel-by-pixel
int compare_images(const bmp_img* img1, const bmp_img* img2) {
    // Check if the dimensions of the images match
    if (img1->img_header.biWidth != img2->img_header.biWidth || img1->img_header.biHeight != img2->img_header.biHeight) {
        printf("Images have different dimensions!\n");
        return -1; // Dimensions are different
    }

    int width = img1->img_header.biWidth;
    int height = img1->img_header.biHeight;

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

void apply_filter(bmp_img *input_img, bmp_img *output_img, int width, int height, )
{
    int x, y, filterX, filterY, imageX, imageY;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            int red = 0, green = 0, blue = 0;

            for (filterY = 0; filterY < FILTER_SIDE_SIZE; filterY++) {
                for (filterX = 0; filterX < FILTER_SIDE_SIZE; filterX++) {
                    imageX = (x + filterX - PADDING + width) % width;
                    imageY = (y + filterY - PADDING + height) % height;

                    // Check if the pixel is within bounds
                    if (imageX >= 0 && imageX < width && imageY >= 0 && imageY < height) {
                        bmp_pixel orig_pixel = input_img->img_pixels[imageY][imageX];
                        int weight = motion_blur[filterY][filterX];

                        // Multiply the pixel value with the filter weight
                        red += orig_pixel.red * weight;
                        green += orig_pixel.green * weight;
                        blue += orig_pixel.blue * weight;
                    }
                }
            }

            // Normalize the values
			output_img->img_pixels[y][x].red = fmin(fmax((int)(red * factor + bias), 0), 255);
            output_img->img_pixels[y][x].green = fmin(fmax((int)(green * factor + bias), 0), 255);
            output_img->img_pixels[y][x].blue = fmin(fmax((int)(blue * factor + bias), 0), 255);
        }
    }
}

int main(int argc, char *argv[]) {
    bmp_img img, img_result;
    enum bmp_error status;
	char output_filepath[MAX_PATH_LEN];
	char input_filepath[MAX_PATH_LEN];

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

	if (strcmp(filter_type, "mb") == 0) {
		apply_filter(&img, &img_result, width, height, motion_blur, 9);
	} else if (strcmp(filter_type, "bb") == 0) {
		apply_filter(&img, &img_result, width, height, blur, 5);
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

