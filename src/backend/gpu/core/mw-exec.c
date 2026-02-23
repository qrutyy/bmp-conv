// SPDX-License-Identifier: GPL-3.0-or-later

#ifdef USE_OPENCL

#include "libbmp/libbmp.h"
#include "logger/log.h"
#include "mw-exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif
#include "utils/threads-general.h"
#include "backend/gpu/utils/utils.h"

void opencl_get_device_info(cl_device_id device)
{
	cl_uint max_dims;
	cl_int err = clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(max_dims), &max_dims, NULL);
	if (err != CL_SUCCESS) {
		log_info("Failed to get max work item dimensions");
		return;
	}

	size_t *work_item_sizes = malloc(max_dims * sizeof(size_t));
	
	err = clGetDeviceInfo(device,
					CL_DEVICE_MAX_WORK_ITEM_SIZES,
					max_dims * sizeof(size_t),
					work_item_sizes,
					NULL);
	
	if (err != CL_SUCCESS) {
		log_info("Failed to get max work item sizes");
		free(work_item_sizes);
		return;
	}

	for (cl_uint i = 0; i < max_dims; i++)
		log_info("CL_DEVICE_MAX_WORK_ITEM_SIZES[%u] = %zu", i, work_item_sizes[i]);

	free(work_item_sizes);
}

double opencl_execute_basic_computation(struct img_spec *img_spec, struct p_args *args, struct filter_mix *filters)
{
    cl_int err;
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;
    cl_mem input_buf, output_buf, weights_buf;

    err = clGetPlatformIDs(1, &platform, NULL);
    if (err != CL_SUCCESS) { log_error("Failed to get platform"); return 0; }

    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    if (err != CL_SUCCESS) {
         err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &device, NULL);
         if (err != CL_SUCCESS) { log_error("Failed to get device"); return 0; }
    }

	opencl_get_device_info(device);

    context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    if (err != CL_SUCCESS) { log_error("Failed to create context"); return 0; }

    queue = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &err);
    if (err != CL_SUCCESS) { log_error("Failed to create command queue"); return 0; }

    // 2. Build Kernel
    char* source = read_kernel_source("src/backend/gpu/core/mw-kernels.cl");
    if (!source) { log_error("Failed to read kernel source"); return 0; }

    program = clCreateProgramWithSource(context, 1, (const char**)&source, NULL, &err);
    free(source);
    if (err != CL_SUCCESS) { log_error("Failed to create program"); return 0; }

    err = clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        size_t log_size;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char* log = malloc(log_size);
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        log_error("Build Log:\n%s", log);
        free(log);
        return 0;
    }

	if (strcmp(args->compute_cfg.filter_type, "mm") == 0)
		kernel = clCreateKernel(program, "apply_mm_filter_kernel", &err);
	else
		kernel = clCreateKernel(program, "apply_filter_kernel", &err);

    if (err != CL_SUCCESS) { log_error("Failed to create kernel"); return 0; }

    // Prepare Data
    int width = img_spec->dim->width;
    int height = img_spec->dim->height;
    int num_pixels = width * height;
    size_t img_size_bytes = (size_t)num_pixels * 3 * sizeof(unsigned char); // Explicitly 3 bytes per pixel

    unsigned char* flat_input = malloc(img_size_bytes);
    if (!flat_input) { log_error("Malloc failed"); return 0; }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            size_t idx = ((size_t)y * width + x) * 3;
            flat_input[idx]     = img_spec->input->img_pixels[y][x].blue;
            flat_input[idx + 1] = img_spec->input->img_pixels[y][x].green;
            flat_input[idx + 2] = img_spec->input->img_pixels[y][x].red;
        }
    }

    // Create Buffers
    input_buf = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, img_size_bytes, flat_input, &err);
    if (err != CL_SUCCESS) { log_error("Failed to create input buffer"); free(flat_input); return 0; }

    output_buf = clCreateBuffer(context, CL_MEM_WRITE_ONLY, img_size_bytes, NULL, &err);
    if (err != CL_SUCCESS) { log_error("Failed to create output buffer"); free(flat_input); return 0; }

    struct filter* f = get_filter_by_name(filters, args->compute_cfg.filter_type);
    if (!f) { log_error("Unknown filter type: %s", args->compute_cfg.filter_type); return 0; }

    size_t weights_count = f->size * f->size;
    size_t weights_size = weights_count * sizeof(float);
    float* flat_weights = malloc(weights_size);
    if (!flat_weights) { log_error("Malloc failed"); return 0; }

    for (int i = 0; i < f->size; i++) {
        for (int j = 0; j < f->size; j++) {
            flat_weights[i * f->size + j] = (float)f->filter_arr[i][j];
        }
    }

    weights_buf = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, weights_size, flat_weights, &err);
    if (err != CL_SUCCESS) { log_error("Failed to create weights buffer"); free(flat_weights); return 0; }
    
    // Free weights buffer immediately
    free(flat_weights);
    flat_weights = NULL;

	float factor_f = (float)f->factor;
    float bias_f = (float)f->bias;
	err  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &input_buf);
	err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &output_buf);
	err |= clSetKernelArg(kernel, 2, sizeof(int), &width);
	err |= clSetKernelArg(kernel, 3, sizeof(int), &height);
	err |= clSetKernelArg(kernel, 4, sizeof(cl_mem), &weights_buf);
	err |= clSetKernelArg(kernel, 5, sizeof(int), &f->size);
	err |= clSetKernelArg(kernel, 6, sizeof(float), &factor_f);
	err |= clSetKernelArg(kernel, 7, sizeof(float), &bias_f);


    if (err != CL_SUCCESS) { log_error("Failed to set kernel args"); return 0; }

    // Enqueue (1 work item per 1 pixel). Optional: --block sets OpenCL local work group size.
    // When block == 0: local_ptr = NULL → implementation chooses work group size automatically.
    size_t global_work_size[2];
    size_t local_work_size[2];
    size_t *local_ptr = NULL;
    int block = args->compute_cfg.block_size > 0 ? args->compute_cfg.block_size : 0;

    if (block > 0) {
		/* TODO add row row splitting */
        size_t max_work_group_size;
        err = clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(max_work_group_size), &max_work_group_size, NULL);
        if (err != CL_SUCCESS) { log_error("Failed to get max work group size"); return 0; }

        if ((size_t)block * block > max_work_group_size) {
             log_error("Error: Block size %d results in work group size %d which exceeds device limit %zu", block, block*block, max_work_group_size);
             return 0;
        }

	    global_work_size[0] = (size_t)((width + block - 1) / block) * (size_t)block;
	    global_work_size[1] = (size_t)((height + block - 1) / block) * (size_t)block;
	    local_work_size[0] = (size_t)block;
	    local_work_size[1] = (size_t)block;
	    local_ptr = local_work_size;
    } else {
	    global_work_size[0] = (size_t)width;
	    global_work_size[1] = (size_t)height;
    }

    cl_event event;
    err = clEnqueueNDRangeKernel(queue, kernel, 2, NULL, global_work_size, local_ptr, 0, NULL, &event);
    if (err != CL_SUCCESS) {
		log_error("Failed to enqueue kernel %d", err);
		return 0;
	}

    clWaitForEvents(1, &event);

    cl_ulong start, end;
    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(start), &start, NULL);
    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(end), &end, NULL);
    double time_seconds = (double)(end - start) / 1e9;

    // Read Result
	unsigned char* flat_output = malloc(img_size_bytes);
    err = clEnqueueReadBuffer(queue, output_buf, CL_TRUE, 0, img_size_bytes, flat_output, 0, NULL, NULL);
    if (err != CL_SUCCESS) { log_error("Failed to read buffer: %d", err); free(flat_input); free(flat_weights); free(flat_output); return 0; }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            size_t idx = ((size_t)y * width + x) * 3;
            // Unpack back to struct
            img_spec->output->img_pixels[y][x].blue  = flat_output[idx];
            img_spec->output->img_pixels[y][x].green = flat_output[idx + 1];
            img_spec->output->img_pixels[y][x].red   = flat_output[idx + 2];
        }
    }

    free(flat_output);
    clReleaseMemObject(input_buf);
    clReleaseMemObject(output_buf);
    clReleaseMemObject(weights_buf);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    return time_seconds;
}

#endif
