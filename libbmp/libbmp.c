/* Copyright 2016 - 2017 Marc Volker Dickmann
 * Project: LibBMP
 */
#include <stdio.h>
#include <stdlib.h>
#include "libbmp.h"
#include "logger/log.h"

// BMP_HEADER

void bmp_header_init_df(bmp_header *header, const int width, const int height)
{
	header->bfSize = (sizeof(bmp_pixel) * width + BMP_GET_PADDING(width)) * abs(height);
	header->bfReserved = 0;
	header->bfOffBits = 54;
	header->biSize = 40;
	header->biWidth = width;
	header->biHeight = height;
	header->biPlanes = 1;
	header->biBitCount = 24;
	header->biCompression = 0;
	header->biSizeImage = 0;
	header->biXPelsPerMeter = 0;
	header->biYPelsPerMeter = 0;
	header->biClrUsed = 0;
	header->biClrImportant = 0;
}

enum bmp_error bmp_header_write(const bmp_header *header, FILE *img_file)
{
	if (header == NULL) {
		return BMP_HEADER_NOT_INITIALIZED;
	} else if (img_file == NULL) {
		return BMP_FILE_NOT_OPENED;
	}

	// Since an adress must be passed to fwrite, create a variable!
	const unsigned short magic = BMP_MAGIC;
	fwrite(&magic, sizeof(magic), 1, img_file);

	// Use the type instead of the variable because its a pointer!
	fwrite(header, sizeof(bmp_header), 1, img_file);
	return BMP_OK;
}

enum bmp_error bmp_header_read(bmp_header *header, FILE *img_file)
{
	if (img_file == NULL) {
		return BMP_FILE_NOT_OPENED;
	}

	// Since an adress must be passed to fread, create a variable!
	unsigned short magic;

	// Check if its an bmp file by comparing the magic nbr:
	if (fread(&magic, sizeof(magic), 1, img_file) != 1 || magic != BMP_MAGIC) {
		return BMP_INVALID_FILE;
	}

	if (fread(header, sizeof(bmp_header), 1, img_file) != 1) {
		return BMP_ERROR;
	}

	return BMP_OK;
}

// BMP_PIXEL

void bmp_pixel_init(bmp_pixel *pxl, const unsigned char red, const unsigned char green, const unsigned char blue)
{
	pxl->red = red;
	pxl->green = green;
	pxl->blue = blue;
}

// BMP_IMG

void bmp_img_alloc(bmp_img *img)
{
	const size_t h = abs(img->img_header.biHeight);

	// Allocate the required memory for the pixels:
	img->img_pixels = malloc(sizeof(bmp_pixel *) * h);

	for (size_t y = 0; y < h; y++) {
		img->img_pixels[y] = malloc(sizeof(bmp_pixel) * img->img_header.biWidth);
	}
}

void *bmp_img_pixel_alloc(size_t height, size_t width)
{
	bmp_pixel **img_pixels = malloc(sizeof(bmp_pixel *) * height);
	if (!img_pixels) {
		log_error("Memory allocation failed");
		return NULL;
	}

	for (size_t y = 0; y < height; y++) {
		img_pixels[y] = malloc(sizeof(bmp_pixel) * width);
		if (!img_pixels[y]) {
			log_error("Memory allocation failed");
			return NULL;
		}
	}
	return img_pixels;
}

void bmp_img_init_df(bmp_img *img, const int width, const int height)
{
	// INIT the header with default values:
	bmp_header_init_df(&img->img_header, width, height);
	bmp_img_alloc(img);
}

void bmp_img_free(bmp_img *img)
{
	const size_t h = abs(img->img_header.biHeight);

	for (size_t y = 0; y < h; y++) {
		free(img->img_pixels[y]);
	}
	free(img->img_pixels);
}

enum bmp_error bmp_img_write(const bmp_img *img, const char *filename)
{
	FILE *img_file = fopen(filename, "wb");

	if (img_file == NULL) {
		return BMP_FILE_NOT_OPENED;
	}

	// NOTE: This way the correct error code could be returned.
	const enum bmp_error err = bmp_header_write(&img->img_header, img_file);

	if (err != BMP_OK) {
		// ERROR: Could'nt write the header!
		fclose(img_file);
		return err;
	}

	// Select the mode (bottom-up or top-down):
	const size_t h = abs(img->img_header.biHeight);
	const size_t offset = (img->img_header.biHeight > 0 ? h - 1 : 0);

	// Create the padding:
	const unsigned char padding[3] = { '\0', '\0', '\0' };

	// Write the content:
	for (size_t y = 0; y < h; y++) {
		// Write a whole row of pixels to the file:
		fwrite(img->img_pixels[offset - y], sizeof(bmp_pixel), img->img_header.biWidth, img_file);

		// Write the padding for the row!
		fwrite(padding, sizeof(unsigned char), BMP_GET_PADDING(img->img_header.biWidth), img_file);
	}

	// NOTE: All good!
	fclose(img_file);
	return BMP_OK;
}

enum bmp_error bmp_img_read(bmp_img *img, const char *filename)
{
	FILE *img_file = fopen(filename, "rb");

	if (img_file == NULL) {
		return BMP_FILE_NOT_OPENED;
	}

	// NOTE: This way the correct error code can be returned.
	const enum bmp_error err = bmp_header_read(&img->img_header, img_file);

	if (err != BMP_OK) {
		// ERROR: Could'nt read the image header!
		fclose(img_file);
		return err;
	}

	bmp_img_alloc(img);

	// Select the mode (bottom-up or top-down):
	const size_t h = abs(img->img_header.biHeight);
	const size_t offset = (img->img_header.biHeight > 0 ? h - 1 : 0);
	const size_t padding = BMP_GET_PADDING(img->img_header.biWidth);

	// Needed to compare the return value of fread
	const size_t items = img->img_header.biWidth;

	// Read the content:
	for (size_t y = 0; y < h; y++) {
		// Read a whole row of pixels from the file:
		if (fread(img->img_pixels[offset - y], sizeof(bmp_pixel), items, img_file) != items) {
			fclose(img_file);
			return BMP_ERROR;
		}

		// Skip the padding:
		fseek(img_file, padding, SEEK_CUR);
	}

	// NOTE: All good!
	fclose(img_file);
	return BMP_OK;
}

void bmp_print_header_data(const bmp_header *header)
{
	if (header == NULL) {
		log_info("Error: print_bmp_header_data called with NULL header pointer.");
		return;
	}

	log_info("--- BMP Header Dump ---");

	log_info("File Header Fields:");
	log_info("  bfSize         : %u bytes", header->bfSize);
	log_info("  bfReserved     : %u", header->bfReserved);
	log_info("  bfOffBits      : %u", header->bfOffBits);

	log_info("-----------------------");

	log_info("Info Header Fields (BITMAPINFOHEADER):");
	log_info("  biSize         : %u bytes", header->biSize);
	log_info("  biWidth        : %d pixels", header->biWidth);
	log_info("  biHeight       : %d pixels", header->biHeight);
	log_info("  biPlanes       : %hu", header->biPlanes);
	log_info("  biBitCount     : %hu bits/pixel", header->biBitCount);
	log_info("  biCompression  : %u", header->biCompression); // Could add interpretation (e.g., 0=RGB, 1=RLE8...)
	log_info("  biSizeImage    : %u bytes", header->biSizeImage);
	log_info("  biXPelsPerMeter: %d", header->biXPelsPerMeter);
	log_info("  biYPelsPerMeter: %d", header->biYPelsPerMeter);
	log_info("  biClrUsed      : %u", header->biClrUsed);
	log_info("  biClrImportant : %u", header->biClrImportant);

	log_info("--- End BMP Header Dump ---");
}

int bmp_compare_images(const bmp_img *img1, const bmp_img *img2)
{
	int width = 0, height = 0;
	bool pixels1_exist = false, pixels2_exist = false;
	size_t x = 0, y = 0;
	const bmp_pixel *p1 = NULL, *p2 = NULL;

	if (img1 == NULL || img2 == NULL) {
		log_error("Error: Cannot compare NULL bmp_img pointers.\n");
		return -1;
	}

	if (img1->img_header.bfSize != img2->img_header.bfSize || img1->img_header.bfReserved != img2->img_header.bfReserved ||
	    img1->img_header.bfOffBits != img2->img_header.bfOffBits || img1->img_header.biSize != img2->img_header.biSize ||
	    img1->img_header.biWidth != img2->img_header.biWidth || img1->img_header.biHeight != img2->img_header.biHeight ||
	    img1->img_header.biPlanes != img2->img_header.biPlanes || img1->img_header.biBitCount != img2->img_header.biBitCount ||
	    img1->img_header.biCompression != img2->img_header.biCompression || img1->img_header.biSizeImage != img2->img_header.biSizeImage ||
	    img1->img_header.biXPelsPerMeter != img2->img_header.biXPelsPerMeter || img1->img_header.biYPelsPerMeter != img2->img_header.biYPelsPerMeter ||
	    img1->img_header.biClrUsed != img2->img_header.biClrUsed || img1->img_header.biClrImportant != img2->img_header.biClrImportant) {
		return 1;
	}

	width = img1->img_header.biWidth;
	height = abs(img1->img_header.biHeight);

	pixels1_exist = (img1->img_pixels != NULL);
	pixels2_exist = (img2->img_pixels != NULL);

	if (pixels1_exist != pixels2_exist) {
		return 1; // One has pixels, the other doesn't
	}

	if (!pixels1_exist) {
		return 0;
	}

	for (y = 0; y < (size_t)height; ++y) {
		if (img1->img_pixels[y] == NULL || img2->img_pixels[y] == NULL) {
			log_error("Error: Found NULL pixel row pointer during comparison at y=%d.\n", y);
			return 1;
		}

		for (x = 0; x < (size_t)width; ++x) {
			p1 = &img1->img_pixels[y][x];
			p2 = &img2->img_pixels[y][x];
			if (p1->red != p2->red || p1->green != p2->green || p1->blue != p2->blue) {
				log_info("Difference in pixels at x:%u y:%u\n P1: %u:%u:%u P2: %u:%u:%u", x, y, p1->red, p1->green, p1->blue, p2->red, p2->green, p2->blue);
				return 1;
			}
		}
	}

	return 0;
}
