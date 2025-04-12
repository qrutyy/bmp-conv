// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../utils/args-parse.h"
#include "../utils/filters.h"
#include "mpi-types.h"

/**
 * Main orchestrator function. Processes the computation with 'by_row' distribution mode.
 * @param rank - numer of the current process 
 * @param size - general number of processes
 * + known params
 *
 * @return Consumed time.
 */
double mpi_process_by_rows(int rank, int size, const struct p_args *args, const struct filter_mix *filters); 

