// SPDX-License-Identifier: GPL-3.0-or-later

struct bmp_pixel __attribute__ ((packed)) {
    unsigned char blue;
    unsigned char green;
    unsigned char red;
};

struct filter_params {
    int size;
    float bias;
    float factor;
};

struct wi_kernel_spec {
	int img_width;
	int img_height;
	int block_size;
};

struct bmp_pixel get_pixel(__global struct bmp_pixel* image, int x, int y, int width, int height) {
    int clampedX = clamp(x, 0, width - 1);
    int clampedY = clamp(y, 0, height - 1);
    return image[clampedY * width + clampedX];
}

__kernel void apply_filter_kernel(
    __global struct bmp_pixel* input_img,
    __global struct bmp_pixel* output_img,
    __constant float* filter_weights,
    struct filter_params params,
	struct wi_kernel_spec spec
) {
    int gid_x = get_global_id(0);
    int gid_y = get_global_id(1);

    int start_row = spec.block_size * gid_y;
    int start_column = spec.block_size * gid_x;

    if (start_row >= spec.img_height || start_column >= spec.img_width) {
        return;
    }

    int end_row = min(start_row + spec.block_size, spec.img_height);
    int end_column = min(start_column + spec.block_size, spec.img_width);

    int padding = params.size / 2;

    for (int y = start_row; y < end_row; y++) {
        for (int x = start_column; x < end_column; x++) {

            float red_acc = 0.0f;
            float green_acc = 0.0f;
            float blue_acc = 0.0f;

            for (int filterY = 0; filterY < params.size; filterY++) {
                for (int filterX = 0; filterX < params.size; filterX++) {
                    int imageX = x + filterX - padding;
                    int imageY = y + filterY - padding;

                    struct bmp_pixel orig_pixel = get_pixel(input_img, imageX, imageY, spec.img_width, spec.img_height);

                    float weight = filter_weights[filterY * params.size + filterX];

                    red_acc += orig_pixel.red * weight;
                    green_acc += orig_pixel.green * weight;
                    blue_acc += orig_pixel.blue * weight;
                }
            }

            struct bmp_pixel out_p;
            out_p.red   = (unsigned char)clamp(red_acc * params.factor + params.bias, 0.0f, 255.0f);
            out_p.green = (unsigned char)clamp(green_acc * params.factor + params.bias, 0.0f, 255.0f);
            out_p.blue  = (unsigned char)clamp(blue_acc * params.factor + params.bias, 0.0f, 255.0f);

            output_img[y * spec.img_width + x] = out_p;
        }
    }
}
