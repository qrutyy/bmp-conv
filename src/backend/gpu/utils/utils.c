// SPDX-License-Identifier: GPL-3.0-or-later

#include "libbmp/libbmp.h"
#include "logger/log.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

char* read_kernel_source(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* source = malloc(len + 1);
    if (source) {
        fread(source, 1, len, f);
        source[len] = '\0';
    }
    fclose(f);
    return source;
}

void check_cl_error(cl_int err, const char* msg) {
    if (err != CL_SUCCESS) {
        log_error("OpenCL Error %d: %s", err, msg);
    }
}
