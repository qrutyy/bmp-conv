// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "libbmp/libbmp.h"
#include <stdint.h>
#include <stddef.h>
#include <mpi.h>

#define BYTES_PER_PIXEL 3
#define ABORT_AND_RETURN(retval)                                                                                                                                                   \
	do {                                                                                                                                                                       \
		fprintf(stderr, "Aborting MPI execution in %s at line %d\n", __FILE__, __LINE__);                                                                                  \
		MPI_Abort(MPI_COMM_WORLD, 1);                                                                                                                                      \
		return (retval);                                                                                                                                                   \
	} while (0)

/**
 * Holds basic MPI context information for the current process.
 */
struct mpi_context {
	uint16_t rank;
	uint16_t size;
};

/**
 * Contains derived information about image data distribution and geometry
 * relevant for communication and local processing within an MPI process.
 */
struct img_comm_data {
	uint8_t halo_size;
	int8_t compute_mode;
	size_t row_stride_bytes; // The number of bytes in a single row/column of the image data. Crucial for calculating memory offsets.
	uint32_t my_start_rc;
	uint32_t my_num_rc; // The number of rows/columns this process is responsible for computing and writing to the output.
	uint32_t send_start_rc; // The starting row/column index of the chunk of data this process needs to receive.
	uint32_t send_num_rc; // The total number of rows/columns this process needs to receive from the original image to perform its computation.
	struct img_dim *dim;
};

/**
 * Bundles arrays used for MPI variable-count collective communication operations.
 * These arrays specify sizes and displacements for data segments being sent or received.
 */
struct mpi_comm_arr {
	int *sendcounts; // sendcounts[i] is the number of elements (bytes) to send to every rank i.
	int *displs; // displs[i] is the displacement for data going to rank i in the packed buffer.
	int *recvcounts; // recvcounts[i] shows how much data were waiting getting back.
	int *recvdispls; // displacement of the i-th gather in the output root buffer.
	int *origdispls; // in comparison with displs[i] - stores the original displacement in the input buffer.
};

/**
 * @brief Holds pointers to the locally relevant pixel data buffers for an MPI process.
 *        This includes the input data chunk (with halo) and the buffer for computed output.
 */
struct mpi_local_data {
	unsigned char *input_pixels;
	unsigned char *output_pixels;
};
