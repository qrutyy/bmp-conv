// SPDX-License-Identifier: GPL-3.0-or-later

__kernel void apply_filter_kernel(
    __global const uchar* input_data,     // Input pixel array (RGB, 3 bytes per pix)
    __global uchar* output_data,          // Output array 
    int width,                            // Image width 
    int height,                           // Image height 
    __constant float* filter_weights,     // Filter addr in  Constant Memory
    int filter_size,                      // Filter size (3, 5, 7...)
    float factor,                         // Factor 
    float bias                            // Bias 
)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    if (x >= width || y >= height) {
        return;
    }

    int padding = filter_size / 2;
    
    float red_acc = 0.0f;
    float green_acc = 0.0f;
    float blue_acc = 0.0f;

    int imageX, imageY;
    int potential_imageX, potential_imageY;

    for (int filterY = 0; filterY < filter_size; filterY++) {
        for (int filterX = 0; filterX < filter_size; filterX++) {
            
            potential_imageX = x + filterX - padding;
            potential_imageY = y + filterY - padding;

            if (potential_imageX < 0) imageX = 0;
            else if (potential_imageX >= width) imageX = width - 1;
            else imageX = potential_imageX;

            if (potential_imageY < 0) imageY = 0;
            else if (potential_imageY >= height) imageY = height - 1;
            else imageY = potential_imageY;

            int pixel_idx = (imageY * width + imageX) * 3;

            uchar r = input_data[pixel_idx];
            uchar g = input_data[pixel_idx + 1];
            uchar b = input_data[pixel_idx + 2];

            float weight = filter_weights[filterY * filter_size + filterX];

            red_acc   += r * weight;
            green_acc += g * weight;
            blue_acc  += b * weight;
        }
    }

    uchar out_r = (uchar)clamp(red_acc * factor + bias, 0.0f, 255.0f);
    uchar out_g = (uchar)clamp(green_acc * factor + bias, 0.0f, 255.0f);
    uchar out_b = (uchar)clamp(blue_acc * factor + bias, 0.0f, 255.0f);

    int out_idx = (y * width + x) * 3;
    
    output_data[out_idx]     = out_r;
    output_data[out_idx + 1] = out_g;
    output_data[out_idx + 2] = out_b;
}

__kernel void apply_mm_filter_kernel(
    __global const uchar* input_data,     // Input pixel array (RGB, 3 bytes per pix)
    __global uchar* output_data,          // Output array 
    int width,                            // Image width 
    int height,                           // Image height 
    __constant float* filter_weights,     // Filter addr in  Constant Memory
    int filter_size,                      // Filter size (3, 5, 7...)
    float factor,                         // Factor 
    float bias                            // Bias 
)
{
}
