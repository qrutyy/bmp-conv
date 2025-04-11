#include "../../libbmp/libbmp.h" 
#include <stdint.h>
#include <stddef.h> 
#include <mpi.h>    

#define MPI_HALO_SIZE 1
#define BYTES_PER_PIXEL 3
#define ABORT_AND_RETURN(retval) \
    do { \
        fprintf(stderr, "Aborting MPI execution in %s at line %d\n", __FILE__, __LINE__); \
        MPI_Abort(MPI_COMM_WORLD, 1); \
        return (retval); \
    } while(0)

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
    size_t row_stride_bytes; // The number of bytes in a single row of the image data (width * bytes_per_pixel + padding, if any). Crucial for calculating memory offsets.
    uint32_t my_start_row;   
    uint32_t my_num_rows;    // The number of rows this process is responsible for computing and writing to the output.
    uint32_t send_start_row; // The starting row index (inclusive, in the *original* image dimensions) of the chunk of data this process needs to receive (including halo/ghost rows).
    uint32_t send_num_rows;  // The total number of rows (including halo/ghost rows) this process needs to receive from the original image to perform its computation.
	struct img_dim *dim;
};

/**
 * Bundles arrays used for MPI variable-count collective communication operations.
 * These arrays specify sizes and displacements for data segments being sent or received.
 */
struct mpi_comm_arr{
    int *sendcounts;    // Array: sendcounts[i] is the number of elements (e.g., bytes) to send to rank i. Used by root in Scatterv, by all in Gatherv/Alltoallv.
    int *displs;        // Array: displs[i] is the displacement (offset in elements from the start of the send buffer) for data going to rank i. Used by root in Scatterv, by all in Gatherv/Alltoallv.
    int *recvcounts;   
    int *recvdispls;    
};

/**
 * @brief Holds pointers to the locally relevant pixel data buffers for an MPI process.
 *        This includes the input data chunk (with halo) and the buffer for computed output.
 */
struct mpi_local_data {
    unsigned char *input_pixels;  
	unsigned char *output_pixels; 
};

/**
 * Parameters specifically for packing data on the root process (rank 0)
 * before performing an MPI_Scatterv operation. It relates the scatter chunks
 * back to the original, complete data source.
 */
struct mpi_pack_params {
	const int *sendcounts;     // Array: sendcounts[i] is the number of elements to be packed and eventually sent to rank i. Same as used in MPI_Scatterv sendcounts
    const int *displs_original; // Array: displs_original[i] is the displacement (offset in elements, e.g., bytes) from the start of the original, complete data buffer where the data chunk for rank i begins. This is used before packing into the contiguous scatter buffer.
};


