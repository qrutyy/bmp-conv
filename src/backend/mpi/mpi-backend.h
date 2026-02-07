// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../compute-backend.h"

#ifdef USE_MPI
/**
 * MPI Compute Backend operations.
 * Implementation of compute_backend_ops for MPI execution.
 */
extern const struct compute_backend_ops mpi_backend_ops;
#endif
