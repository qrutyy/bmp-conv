// C standard includes
#include <stdio.h>

// OpenCL includes
#include <CL/cl.h>

double execute_basic_computation(struct img_spec *img_spec, struct p_args *args, struct filter_mix *filters)
{
    cl_int CL_err = CL_SUCCESS;
    cl_uint numPlatforms = 0;

    CL_err = clGetPlatformIDs( 0, NULL, &numPlatforms );

    if (CL_err == CL_SUCCESS)
        printf("%u platform(s) found\n", numPlatforms);
    else
        printf("clGetPlatformIDs(%i)\n", CL_err);

    return 0;
}
