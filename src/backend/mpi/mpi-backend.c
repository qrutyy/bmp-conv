// SPDX-License-Identifier: GPL-3.0-or-later

#include "mpi-backend.h"
#include "../compute-backend.h"
#include "logger/log.h"
#include "core/exec.h"
#include <mpi.h>
#include <stdlib.h>

#ifdef USE_MPI

struct mpi_backend_data {
	int rank;
	int size;
};

static int mpi_init(struct compute_backend *backend, struct p_args *args)
{
	struct mpi_backend_data *data = NULL;

	if (!backend || !args) {
		log_error("Error: NULL parameter in mpi_init\n");
		return -1;
	}

	data = malloc(sizeof(struct mpi_backend_data));
	if (!data) {
		log_error("Error: Failed to allocate memory for mpi_backend_data\n");
		return -1;
	}

	/* 
     * Using NULL, NULL for argc, argv as we don't have access to them here easily
     * and they are consumed by parse_args already. 
     * Most MPI implementations support this.
     */
	MPI_Init(NULL, NULL);

	MPI_Comm_rank(MPI_COMM_WORLD, &data->rank);
	MPI_Comm_size(MPI_COMM_WORLD, &data->size);

	log_info("MPI Initialized. Rank: %d, Size: %d", data->rank, data->size);

	backend->backend_data = data;
	return 0;
}

static double mpi_process_image(struct compute_backend *backend)
{
	struct mpi_backend_data *data = (struct mpi_backend_data *)backend->backend_data;
	if (!data) {
		log_error("Error: MPI backend data not initialized\n");
		return 0.0;
	}

	return execute_mpi_computation(data->size, data->rank, backend->args, backend->filters);
}

static void mpi_cleanup(struct compute_backend *backend)
{
	if (!backend)
		return;

	if (backend->backend_data) {
		free(backend->backend_data);
		backend->backend_data = NULL;
	}

	MPI_Finalize();
}

static enum conv_backend mpi_get_type(void)
{
	return CONV_BACKEND_MPI;
}

static const char *mpi_get_name(void)
{
	return "MPI";
}

const struct compute_backend_ops mpi_backend_ops = {
	.init = mpi_init,
	.process_image = mpi_process_image,
	.cleanup = mpi_cleanup,
	.get_type = mpi_get_type,
	.get_name = mpi_get_name
};

#endif
