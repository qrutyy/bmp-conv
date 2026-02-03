// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../utils/utils.h"
#include "../utils/threads-general.h"

/**
 * Initializes a thread_spec structure encompassing the whole image dimensions, calls the core computation function, measures the execution time, and cleans up allocated resources.
 *
 * @param img_spec Pointer to the img_spec structure containing input/output image pointers.
 * @param args Pointer to the p_args structure containing program arguments (like filter type).
 * @param filters Pointer to the filter mix data structure used by the computation.
 *
 * @return Time spent (in seconds) for the computation part, or 0.0 on allocation error.
 */
double execute_st_computation(struct img_spec *img_spec, struct p_args *args, void *filters);
