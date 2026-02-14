// SPDX-License-Identifier: GPL-3.0-or-later

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

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

char* read_kernel_source(const char* filename);
void check_cl_error(cl_int err, const char* msg);
