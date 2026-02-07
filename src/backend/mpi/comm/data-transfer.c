// SPDX-License-Identifier: GPL-3.0-or-later

#include "data-transfer.h"
#include "logger/log.h"
#include "libbmp/libbmp.h"
#include "utils/threads-general.h"
#include "backend/mpi/utils/utils.h"
#include "rank0-proc.h"
#include <mpi.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

int8_t mpi_phase_scatter_data(const struct mpi_context *ctx, const struct img_comm_data *comm_data, struct mpi_local_data *local_data, unsigned char *global_send_buffer,
			      const struct mpi_comm_arr *comm_arrays)
{
	if (!ctx->rank) {
		assert(global_send_buffer);
		assert(comm_arrays->sendcounts);
		assert(comm_arrays->displs);
	}

	int8_t setup_status = 0;
	int8_t mpi_rc = MPI_SUCCESS;
	int my_recv_size_scatter = 0;

	log_trace("SCATTER DATA PHASE: \n");

	setup_status = mpi_allocate_local_buffers(ctx, comm_data, local_data);
	if (setup_status != 0) {
		log_error("Failed to allocate buffers");
		MPI_Abort(MPI_COMM_WORLD, 1);
		return -1;
	}

	my_recv_size_scatter = (int)(comm_data->send_num_rc * comm_data->row_stride_bytes); // scatterv requires int type
	if (my_recv_size_scatter <= 0) {
		log_warn("Rank %d: Calculated zero receive size for non-zero rows (%u). Setting to 1.", ctx->rank, comm_data->send_num_rc);
		my_recv_size_scatter = (comm_data->send_num_rc == 0) ? 0 : 1;
	}

	log_debug("Rank %d: Scattering. Expecting %d bytes (%u rows).", ctx->rank, my_recv_size_scatter, comm_data->send_num_rc);

	mpi_rc = MPI_Scatterv(global_send_buffer, // buffer with continuous image we are scattering from
			      comm_arrays->sendcounts, // number of items we are seding for each proc
			      comm_arrays->displs, // displacement in buffer for each ith
			      MPI_UNSIGNED_CHAR, // data type of send buffer el
			      local_data->input_pixels, // address of receive buffer
			      my_recv_size_scatter, // number of el in receive buff
			      MPI_UNSIGNED_CHAR, // recv type
			      0, // rank of sending process
			      MPI_COMM_WORLD);

	log_trace("Finished ScatterV\n");

	if (ctx->rank == 0)
		free(global_send_buffer);

	if (mpi_rc != MPI_SUCCESS) {
		log_error("Rank %d: MPI_Scatterv failed with code %d.", ctx->rank, mpi_rc);
		return -1;
	}

	return 0;
}

int8_t mpi_phase_gather_data(const struct mpi_context *ctx, const struct img_comm_data *comm_data, const struct mpi_local_data *local_data, const struct mpi_comm_arr *comm_arrays,
			     struct img_spec *img_data)
{
	unsigned char *global_recv_buffer = NULL;
	int8_t mpi_rc = MPI_SUCCESS;
	int8_t unpack_status = 0;
	int my_send_size_gather = 0;
	size_t i = 0;

	if (ctx->rank == 0) {
		size_t total_gathered_size = 0;
		if (comm_arrays->recvcounts) { // Check pointer validity
			for (i = 0; i < ctx->size; ++i) {
				total_gathered_size += (size_t)comm_arrays->recvcounts[i];
			}
		}
		if (total_gathered_size > 0) {
			global_recv_buffer = (unsigned char *)malloc(total_gathered_size);
			if (!global_recv_buffer) {
				log_error("Rank 0: Failed to allocate buffer for gathering results (%zu bytes).", total_gathered_size);
				return -1;
			}
		} else {
			global_recv_buffer = NULL;
		}
	}

	my_send_size_gather = (int)(comm_data->my_num_rc * comm_data->row_stride_bytes);
	if (my_send_size_gather <= 0 && comm_data->my_num_rc > 0) {
		log_warn("Rank %d: Calculated zero send size for non-zero rows (%u). Setting to 1.", ctx->rank, comm_data->my_num_rc);
		my_send_size_gather = 1;
	}
	if (my_send_size_gather <= 0 && comm_data->my_num_rc == 0) {
		my_send_size_gather = 0;
	}

	log_debug("Rank %d: Gathering. Sending %d bytes (%u rows).", ctx->rank, my_send_size_gather, comm_data->my_num_rc);

	mpi_rc = MPI_Gatherv(local_data->output_pixels, my_send_size_gather, MPI_UNSIGNED_CHAR, global_recv_buffer, comm_arrays->recvcounts, comm_arrays->recvdispls,
			     MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

	if (mpi_rc != MPI_SUCCESS) {
		log_error("Rank %d: MPI_Gatherv failed with code %d.", ctx->rank, mpi_rc);
		free(global_recv_buffer);
		return -1;
	}

	if (ctx->rank == 0 && global_recv_buffer != NULL) {
		unpack_status = mpi_rank0_unpack_data(global_recv_buffer, img_data, comm_data, ctx, comm_arrays);
		free(global_recv_buffer);
		if (unpack_status != 0) {
			log_error("Rank 0: Failed to unpack gathered data.");
			return -1;
		}
	}

	return 0;
}

// Simply sents img dim to all the prcesses (from rank0)
void mpi_broadcast_metadata(struct img_comm_data *comm_data)
{
	uint16_t mpi_width = comm_data->dim->width;
	uint16_t mpi_height = comm_data->dim->height;
	uint16_t mpi_row_stride = comm_data->row_stride_bytes;

	MPI_Bcast(&mpi_width, 1, MPI_UINT16_T, 0, MPI_COMM_WORLD);
	MPI_Bcast(&mpi_height, 1, MPI_UINT16_T, 0, MPI_COMM_WORLD);
	MPI_Bcast(&mpi_row_stride, 1, MPI_UINT16_T, 0, MPI_COMM_WORLD);

	comm_data->dim->width = mpi_width;
	comm_data->dim->height = mpi_height;
	comm_data->row_stride_bytes = mpi_row_stride;

	log_debug("Successfully broadcasted dim");
}
