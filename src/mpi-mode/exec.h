// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../utils/args-parse.h"
#include "../utils/filters.h"
#include <stdint.h>

// executes the computation function, depending on the type of computation
double execute_mpi_computation(uint8_t size, uint8_t rank, struct p_args *compute_args, struct filter_mix *filters);

