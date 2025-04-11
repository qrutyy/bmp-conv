
// ==========================================================================
// ==                        COLUMN-BASED PROCESSING                       ==
// ==========================================================================
// NOTE: Column-based processing on row-major data is significantly more
// complex due to non-contiguous memory access for columns. Using MPI Derived
// Datatypes is highly recommended for efficiency. The implementation below
// demonstrates this approach.


/**
 * @brief Calculates the distribution of image columns among MPI processes.
 */
static void mpi_calculate_col_distribution(int rank, int size, uint32_t width, uint32_t *my_start_col, uint32_t *my_num_cols) {
    // --- Variable Initialization ---
	uint32_t cols_per_proc = 0;
	uint32_t remainder_cols = 0;
    uint32_t u_width = width;

    // --- Function Body ---
	if (size <= 0) {
        *my_start_col = 0;
        *my_num_cols = (rank == 0) ? u_width : 0;
        return;
    }
    cols_per_proc = u_width / (uint32_t)size;
    remainder_cols = u_width % (uint32_t)size;
    *my_start_col = (uint32_t)rank * cols_per_proc + ((uint32_t)rank < remainder_cols ? (uint32_t)rank : remainder_cols);
    *my_num_cols = cols_per_proc + ((uint32_t)rank < remainder_cols ? 1 : 0);
}


/**
 * @brief Creates an MPI derived datatype representing a block of columns.
 * @details Creates a vector type where each block is a single pixel and the stride
 *          is the full row stride in bytes. This is then resized to represent
 *          multiple columns contiguously in terms of datatype extent.
 * @param height Image height (number of rows = count in vector).
 * @param num_cols Number of columns this datatype represents.
 * @param row_stride_bytes Stride between start of rows in bytes.
 * @param col_type Pointer to store the created MPI_Datatype.
 * @return MPI_SUCCESS or an MPI error code.
 */
static int create_mpi_column_block_type(uint32_t height, uint32_t num_cols, size_t row_stride_bytes, MPI_Datatype *col_type) {
    // --- Variable Initialization ---
    int mpi_rc = MPI_SUCCESS;
    MPI_Datatype pixel_col_vector_type; // Represents a single column of pixels
    MPI_Aint lb, extent; // Lower bound and extent for resizing

    // --- Function Body ---
    if (num_cols == 0 || height == 0) {
        // Cannot create a type for zero columns/rows. Return a predefined type like MPI_BYTE?
        // Or handle this case upstream. For simplicity, let's create MPI_BYTE type.
        // This might need adjustment depending on how Scatterv/Gatherv handle zero counts.
         log_warn("Attempting to create column type for zero cols/height.");
         // MPI_Type_contiguous(0, MPI_BYTE, col_type); // Create an empty type
         *col_type = MPI_BYTE; // Or just use byte? Let's use byte and handle count=0 later.
         return MPI_SUCCESS;
    }

    // 1. Create a vector type for a single column of pixels
    //    count = height (number of rows/pixels in the column)
    //    blocklength = BYTES_PER_PIXEL (bytes per pixel)
    //    stride = row_stride_bytes (bytes between start of rows)
    //    oldtype = MPI_UNSIGNED_CHAR (base type)
    mpi_rc = MPI_Type_vector((int)height,                 /* count */
                             BYTES_PER_PIXEL,         /* blocklength */
                             (int)row_stride_bytes,   /* stride */
                             MPI_UNSIGNED_CHAR,       /* oldtype */
                             &pixel_col_vector_type); /* newtype */
    if (mpi_rc != MPI_SUCCESS) return mpi_rc;

    // 2. Get the true extent of the single column vector type.
    //    The extent might be larger than height * row_stride due to internal padding or alignment.
    //    However, for contiguous columns, we want the extent to be exactly BYTES_PER_PIXEL.
    mpi_rc = MPI_Type_get_extent(pixel_col_vector_type, &lb, &extent);
    if (mpi_rc != MPI_SUCCESS) { MPI_Type_free(&pixel_col_vector_type); return mpi_rc; }

    // 3. Create the final type representing 'num_cols' columns contiguously.
    //    We use MPI_Type_create_resized to set the extent of the single-column vector
    //    to exactly BYTES_PER_PIXEL. This ensures that when we specify a count of 'num_cols'
    //    of this resized type, MPI correctly calculates the displacement to the next
    //    column block as starting immediately after the current pixel.
    mpi_rc = MPI_Type_create_resized(pixel_col_vector_type,
                                     0,                      /* new lower bound */
                                     (MPI_Aint)BYTES_PER_PIXEL, /* new extent */
                                     col_type);               /* resulting type */

    // 4. Free the intermediate vector type
    MPI_Type_free(&pixel_col_vector_type);

    return mpi_rc;
}


/**
 * @brief Sets up arrays and MPI datatypes for column-based Scatterv/Gatherv. (Rank 0 only)
 */
static int mpi_setup_scatter_gather_col_arrays(int rank, int size, uint32_t height, uint32_t width, size_t row_stride_bytes,
                                               int **sendcounts, int **displs, MPI_Datatype **sendtypes,
                                               int **recvcounts, int **recvdispls, MPI_Datatype **recvtypes)
{
    // --- Variable Initialization ---
    uint32_t proc_start_col = 0;
    uint32_t proc_num_cols = 0;
    int proc_send_start_col = 0;
    uint32_t proc_send_end_col = 0;
    uint32_t proc_send_cols = 0;
    int mpi_rc = MPI_SUCCESS;

    // --- Function Body ---
    if (rank != 0) {
        *sendcounts = *displs = *recvcounts = *recvdispls = NULL;
        *sendtypes = *recvtypes = NULL;
        return 0;
    }

    *sendcounts = (int *)malloc((size_t)size * sizeof(int));
    *displs = (int *)malloc((size_t)size * sizeof(int));
    *sendtypes = (MPI_Datatype *)malloc((size_t)size * sizeof(MPI_Datatype));
    *recvcounts = (int *)malloc((size_t)size * sizeof(int));
    *recvdispls = (int *)malloc((size_t)size * sizeof(int));
    *recvtypes = (MPI_Datatype *)malloc((size_t)size * sizeof(MPI_Datatype));

    if (!*sendcounts || !*displs || !*sendtypes || !*recvcounts || !*recvdispls || !*recvtypes) {
        log_error("Rank 0: Failed to allocate memory for column scatter/gather arrays/types.");
        // Free allocated memory
        free(*sendcounts); free(*displs); free(*sendtypes);
        free(*recvcounts); free(*recvdispls); free(*recvtypes);
        *sendcounts = *displs = *recvcounts = *recvdispls = NULL;
        *sendtypes = *recvtypes = NULL;
        return -1;
    }

    for (int i = 0; i < size; ++i) {
        mpi_calculate_col_distribution(i, size, width, &proc_start_col, &proc_num_cols);

        // Calculate send region (including halo columns)
        proc_send_start_col = (int)proc_start_col - MPI_HALO_SIZE;
        proc_send_end_col = proc_start_col + proc_num_cols + MPI_HALO_SIZE;

        // Clamp boundaries
        if (proc_send_start_col < 0) proc_send_start_col = 0;
        if (proc_send_end_col > width) proc_send_end_col = width;
        proc_send_cols = (proc_send_end_col > (uint32_t)proc_send_start_col) ? (proc_send_end_col - (uint32_t)proc_send_start_col) : 0;

        // --- Send Parameters (Scatterv) ---
        // Count: Number of columns to send (including halo)
        (*sendcounts)[i] = (int)proc_send_cols;
        // Displacement: Byte offset to the *first pixel* of the *first column* to send.
        (*displs)[i] = (int)((uint32_t)proc_send_start_col * BYTES_PER_PIXEL); // Offset from row start
        // Type: Derived datatype representing a block of 'proc_send_cols' columns.
        // NOTE: Scatterv doesn't directly support different types per rank easily.
        // A common approach is to use Scatterv with bytes and pack/unpack, OR
        // use point-to-point sends with derived types, OR use a single derived type
        // and adjust counts/displacements carefully. Let's create the type for *receiving*
        // and use point-to-point or pack/unpack logic for sending columns.

        // Let's simplify: Use MPI_Scatterv with bytes, requiring packing on Rank 0.
        // OR We can try using the *single column type* and send 'proc_send_cols' of them,
        // but the displacement needs careful calculation.

        // **Using Derived Type Approach (more advanced):**
        // Create a type representing ONE column block (height * BYTES_PER_PIXEL bytes, stride row_stride_bytes).
        // Then send 'proc_send_cols' instances of this type.
        // Displacement 'displs[i]' would be the byte offset to the first pixel of the starting column.
        mpi_rc = create_mpi_column_block_type(height, 1, row_stride_bytes, &(*sendtypes)[i]); // Type for ONE column
         if (mpi_rc != MPI_SUCCESS) { goto cleanup_error; }
        // Send 'proc_send_cols' number of these types.
         (*sendcounts)[i] = (int)proc_send_cols;
         // Displacement is the byte offset to the start of the first column's first pixel
         (*displs)[i] = (int)((uint32_t)proc_send_start_col * BYTES_PER_PIXEL);

        // --- Receive Parameters (Gatherv) ---
        // Create a type representing the block of 'proc_num_cols' main columns for process 'i'.
        mpi_rc = create_mpi_column_block_type(height, 1, row_stride_bytes, &(*recvtypes)[i]); // Type for ONE column
         if (mpi_rc != MPI_SUCCESS) { goto cleanup_error; }
        // Receive 'proc_num_cols' number of these types.
        (*recvcounts)[i] = (int)proc_num_cols;
        // Displacement is the byte offset to the start of the first *main* column's first pixel for process 'i'.
        (*recvdispls)[i] = (int)(proc_start_col * BYTES_PER_PIXEL);
    }

    return 0; // Success

cleanup_error:
    log_error("Rank 0: Failed to create MPI column derived types (MPI Error %d).", mpi_rc);
    // Free any types created so far and allocated memory
    for(int j=0; j<size; ++j) {
        if(i>j && (*sendtypes)[j] != MPI_DATATYPE_NULL && (*sendtypes)[j] != MPI_BYTE) MPI_Type_free(&(*sendtypes)[j]);
        if(i>=j && (*recvtypes)[j] != MPI_DATATYPE_NULL && (*recvtypes)[j] != MPI_BYTE) MPI_Type_free(&(*recvtypes)[j]);
    }
    free(*sendcounts); free(*displs); free(*sendtypes);
    free(*recvcounts); free(*recvdispls); free(*recvtypes);
     *sendcounts = *displs = *recvcounts = *recvdispls = NULL;
    *sendtypes = *recvtypes = NULL;
    return -1;

}

/**
 * @brief Allocates local buffers for column-based processing.
 * @details The buffers are flat, but conceptually hold column data.
 */
static int mpi_allocate_local_col_buffers(int rank, uint32_t height, uint32_t my_num_cols, uint32_t recv_num_cols, unsigned char **my_input_col_pixels, unsigned char **my_output_col_pixels) {
    // --- Variable Initialization ---
    size_t input_buf_size_bytes = 0;
    size_t output_buf_size_bytes = 0;

    // --- Function Body ---
    // Buffers will store columns contiguously (height * BYTES_PER_PIXEL per column)
    input_buf_size_bytes = (size_t)recv_num_cols * height * BYTES_PER_PIXEL;
    output_buf_size_bytes = (size_t)my_num_cols * height * BYTES_PER_PIXEL;

    *my_input_col_pixels = (unsigned char *)malloc(input_buf_size_bytes > 0 ? input_buf_size_bytes : 1);
    *my_output_col_pixels = (unsigned char *)malloc(output_buf_size_bytes > 0 ? output_buf_size_bytes : 1);

    if (!*my_input_col_pixels || !*my_output_col_pixels) {
        log_error("Rank %d: Failed to allocate local column pixel buffers (in: %zu B, out: %zu B).", rank, input_buf_size_bytes, output_buf_size_bytes);
        free(*my_input_col_pixels);
        free(*my_output_col_pixels);
        *my_input_col_pixels = *my_output_col_pixels = NULL;
        return -1;
    }
    return 0;
}


/**
 * @brief Performs filtering on the local column data region.
 * @details Reads from the input column buffer (incl. halo) and writes to the output column buffer.
 *          Requires careful indexing to access horizontal and vertical neighbors.
 */
static void mpi_process_local_col_region(const unsigned char *input_col_pixels, unsigned char *output_col_pixels,
                                         uint32_t height, uint32_t local_width, /* my_num_cols */
                                         uint32_t total_recv_width, /* recv_num_cols */
                                         uint32_t halo_size,
                                         const struct p_args *compute_args, const struct filter_mix *filters)
{
    // --- Variable Initialization ---
    size_t input_col_stride = (size_t)height * BYTES_PER_PIXEL; // Bytes per column in the flat buffer
    size_t output_col_stride = input_col_stride; // Same layout
    const unsigned char *p_center = NULL, *p_left = NULL, *p_right = NULL, *p_up = NULL, *p_down = NULL;

    // --- Basic Checks ---
     if (!input_col_pixels || !output_col_pixels || local_width <= 0 || height <= 0) {
        log_warn("Rank ?: Skipping local column processing due to invalid inputs/zero dimensions.");
        return;
    }

    // --- Filter Logic ---
    // Iterate through each *main* column assigned to this process
    for (uint32_t x_local = 0; x_local < local_width; ++x_local) {
        // Calculate the index of the corresponding column in the input buffer (offset by halo)
        uint32_t x_input = x_local + halo_size;

        // Iterate through each row (pixel within the column)
        for (uint32_t y = 0; y < height; ++y) {

            // --- Calculate pointers to neighboring pixels ---
            // Note: input_col_pixels stores columns contiguously.

            // Center pixel in the input buffer
            p_center = input_col_pixels + x_input * input_col_stride + y * BYTES_PER_PIXEL;

            // Vertical neighbors (within the same column block in the buffer)
            p_up = (y > 0) ? (p_center - BYTES_PER_PIXEL) : p_center; // Clamp top
            p_down = (y < height - 1) ? (p_center + BYTES_PER_PIXEL) : p_center; // Clamp bottom

            // Horizontal neighbors (in adjacent column blocks in the buffer)
            p_left = (x_input > 0) ? (p_center - input_col_stride) : p_center; // Clamp left
            p_right = (x_input < total_recv_width - 1) ? (p_center + input_col_stride) : p_center; // Clamp right

            // Diagonal neighbors can be derived from these

            // --- Apply Filter ---
            // *******************************************************************
            // *    <<< REPLACE THIS WITH YOUR ACTUAL FILTERING LOGIC >>>        *
            // *    Use p_center, p_left, p_right, p_up, p_down etc. to read   *
            // *    neighboring pixel values (e.g., p_center[0] for Blue).       *
            // *******************************************************************
            // Example: Simple copy
            unsigned char *p_out = output_col_pixels + x_local * output_col_stride + y * BYTES_PER_PIXEL;
            p_out[0] = p_center[0]; // B
            p_out[1] = p_center[1]; // G
            p_out[2] = p_center[2]; // R

            // *******************************************************************
            // *    <<< END OF FILTERING LOGIC PLACEHOLDER >>>                   *
            // *******************************************************************
        }
    }
}


/**
 * @brief Main orchestration function for MPI processing decomposed by columns.
 * @param rank Rank of the current MPI process.
 * @param size Total number of MPI processes.
 * @param args Parsed command-line arguments.
 * @param filters Filter mix structure (if used).
 * @return Total computation time (measured on rank 0), or -1.0 on error.
 */
double mpi_process_by_columns(int rank, int size, const struct p_args *args, const struct filter_mix *filters) {
     // --- Variable Initialization ---
    bmp_img img = {0};
    bmp_img img_result = {0};
    uint32_t width = 0, height = 0;
    double start_time = 0.0, total_time = -1.0;
    uint32_t my_start_col = 0, my_num_cols = 0, recv_num_cols = 0;
    uint32_t recv_start_col = 0, recv_end_col = 0;

    int *sendcounts = NULL, *displs = NULL, *recvcounts = NULL, *recvdispls = NULL;
    MPI_Datatype *sendtypes = NULL, *recvtypes = NULL; // For derived type approach
    MPI_Datatype column_send_type = MPI_DATATYPE_NULL; // Type used for sending
    MPI_Datatype column_recv_type = MPI_DATATYPE_NULL; // Type used for receiving


    unsigned char *my_input_col_pixels = NULL; // Local buffer for received columns (flat)
    unsigned char *my_output_col_pixels = NULL;// Local buffer for processed columns (flat)
    // Rank 0 needs the original image data directly if using derived types for scatter/gather
    // No separate global packed buffer needed if derived types work correctly.

    size_t row_stride_bytes = 0;
    int mpi_rc = MPI_SUCCESS;
    int setup_status = 0;

    // --- Input Validation ---
    if (!args || !args->input_filename[0]) {
        if (rank == 0) log_error("Rank 0: Error: Missing input filename for MPI mode.");
        MPI_Abort(MPI_COMM_WORLD, 1);
        return -1.0;
    }

    // --- Rank 0 Initialization ---
    if (rank == 0) {
        setup_status = mpi_rank0_initialize(&img, &img_result, &width, &height, &start_time, args->input_filename[0]);
        if (setup_status != 0) { MPI_Abort(MPI_COMM_WORLD, 1); return -1.0; }
        row_stride_bytes = (size_t)(((width * BYTES_PER_PIXEL * 8 + 31) / 32) * 4);
        // row_stride_bytes = (size_t)width * BYTES_PER_PIXEL; // Adjust if needed
    }

    // --- Broadcast Metadata ---
    mpi_broadcast_metadata(&width, &height);
    if (rank != 0) {
        row_stride_bytes = (size_t)(((width * BYTES_PER_PIXEL * 8 + 31) / 32) * 4);
        // row_stride_bytes = (size_t)width * BYTES_PER_PIXEL; // Adjust if needed
    }
    if (width <= 0 || height <= 0) {
        log_error("Rank %d: Received invalid image dimensions (%ux%u). Aborting.", rank, width, height);
        MPI_Abort(MPI_COMM_WORLD, 1); return -1.0;
    }


    // --- Calculate Local Column Distribution and Receive Ranges ---
    mpi_calculate_col_distribution(rank, size, width, &my_start_col, &my_num_cols);
    recv_start_col = (uint32_t)((int)my_start_col - MPI_HALO_SIZE);
    recv_end_col = my_start_col + my_num_cols + MPI_HALO_SIZE;
    if ((int)recv_start_col < 0) recv_start_col = 0;
    if (recv_end_col > width) recv_end_col = width;
    recv_num_cols = (recv_end_col > recv_start_col) ? (recv_end_col - recv_start_col) : 0;


    // --- Rank 0: Setup Scatter/Gather Arrays and Types ---
    setup_status = mpi_setup_scatter_gather_col_arrays(rank, size, height, width, row_stride_bytes,
                                                       &sendcounts, &displs, &sendtypes,
                                                       &recvcounts, &recvdispls, &recvtypes);
    if (setup_status != 0) { MPI_Abort(MPI_COMM_WORLD, 1); return -1.0; }


    // --- Create Local Derived Types for Sending/Receiving ---
    // Scatterv/Gatherv expect a *single* type argument for send/recv type per call.
    // However, the counts/displacements are relative to the *root's* view.
    // For the *local* process, we need a type describing the data block it *receives*
    // and *sends*. This is tricky with Scatterv/Gatherv when types differ per rank.

    // **Alternative: Using MPI_Send/Recv loop (Simpler but less efficient)**
    // **Alternative: Pack/Unpack (Similar to row-based)**
    // **Let's proceed assuming Scatterv/Gatherv works with the types defined in setup:**
    // We need the specific type this rank will use locally.
    if (rank == 0) { // Root uses the types array
       // Root doesn't necessarily need a single local type unless sending to itself
    } else { // Non-root needs its specific receive type
       mpi_rc = create_mpi_column_block_type(height, 1, row_stride_bytes, &column_recv_type);
       if (mpi_rc != MPI_SUCCESS) { log_error("Rank %d: Failed to create local column receive type.", rank); goto col_cleanup_abort; }
    }
    // Type for sending back results (main columns only)
    mpi_rc = create_mpi_column_block_type(height, 1, row_stride_bytes, &column_send_type);
    if (mpi_rc != MPI_SUCCESS) { log_error("Rank %d: Failed to create local column send type.", rank); goto col_cleanup_abort;}


    // --- Allocate Local Buffers ---
    setup_status = mpi_allocate_local_col_buffers(rank, height, my_num_cols, recv_num_cols, &my_input_col_pixels, &my_output_col_pixels);
     if (setup_status != 0) { goto col_cleanup_abort; }

    // --- Scatter Data ---
    // Pointer to the start of the *entire* image data on Rank 0
    // Assumes bmp_img_read allocated pixels contiguously or provides such a pointer.
    // THIS IS A CRITICAL ASSUMPTION. If img->img_pixels[0] isn't the start of a
    // contiguous block for the whole image, this won't work without packing first.
    void* scatter_base_ptr = NULL;
    if (rank == 0) {
        if (img.img_pixels && img.img_pixels[0]) {
             scatter_base_ptr = img.img_pixels[0]; // Assumes row 0 pointer is start of contiguous block
        } else {
            log_error("Rank 0: Cannot get base pointer for scattering column data.");
            goto col_cleanup_abort;
        }
    }

    // Scatterv with Derived Types. Sends 'sendcounts[i]' blocks described by 'sendtypes[i]'
    // starting from byte offset 'displs[i]' in the root's buffer.
    // Each rank receives 'recv_num_cols' blocks of its 'column_recv_type'.
    // IMPORTANT: Scatterv expects a SINGLE recvtype. Gatherv expects a SINGLE sendtype.
    // This derived type approach might only work correctly with point-to-point or
    // if all processes receive/send the same *number* of the *same* base type, which
    // isn't true here due to varying column counts and halo.

    // *** Switching to Pack/Unpack for Columns due to Scatterv/Gatherv limitations ***
    // Free derived types if created
     if (column_recv_type != MPI_DATATYPE_NULL && column_recv_type != MPI_BYTE) MPI_Type_free(&column_recv_type);
     if (column_send_type != MPI_DATATYPE_NULL && column_send_type != MPI_BYTE) MPI_Type_free(&column_send_type);
     if (rank == 0) {
         for (int i = 0; i < size; ++i) {
            if (sendtypes[i] != MPI_DATATYPE_NULL && sendtypes[i] != MPI_BYTE) MPI_Type_free(&sendtypes[i]);
            if (recvtypes[i] != MPI_DATATYPE_NULL && recvtypes[i] != MPI_BYTE) MPI_Type_free(&recvtypes[i]);
         }
         free(sendtypes); free(recvtypes); sendtypes = recvtypes = NULL;
         // Recalculate counts/displacements for BYTES now
         free(sendcounts); free(displs); free(recvcounts); free(recvdispls);
         // Re-run setup but calculate byte counts/displs for packed buffer
         log_info("Switching column processing to pack/unpack due to MPI limitations.");
         // THIS PART NEEDS REIMPLEMENTATION:
         // 1. Calculate byte send/recv counts including halo/main cols.
         // 2. Calculate displacements for packed buffer.
         // 3. Rank 0: Pack columns into global_send_buffer.
         // 4. MPI_Scatterv with MPI_UNSIGNED_CHAR.
         // 5. Local processing on flat buffer.
         // 6. MPI_Gatherv with MPI_UNSIGNED_CHAR into global_recv_buffer.
         // 7. Rank 0: Unpack columns from global_recv_buffer to img_result.
         log_error("Column processing with pack/unpack is not fully implemented yet.");
         goto col_cleanup_abort; // Abort for now
     }


    // --- Local Computation ---
    if (my_num_cols > 0 && recv_num_cols > 0) {
        mpi_process_local_col_region(my_input_col_pixels, my_output_col_pixels, height, my_num_cols, recv_num_cols, MPI_HALO_SIZE, args, filters);
    } else {
        log_debug("Rank %d: Skipping local column processing as my_num_cols or recv_num_cols is 0.", rank);
    }


    // --- Gather Results ---
    // Similar Scatterv issue applies to Gatherv with derived types differing per rank.
    // Need pack/unpack or point-to-point.


    // --- Cleanup ---
col_cleanup:
    free(my_input_col_pixels); my_input_col_pixels = NULL;
    free(my_output_col_pixels); my_output_col_pixels = NULL;
     if (column_recv_type != MPI_DATATYPE_NULL && column_recv_type != MPI_BYTE) MPI_Type_free(&column_recv_type);
     if (column_send_type != MPI_DATATYPE_NULL && column_send_type != MPI_BYTE) MPI_Type_free(&column_send_type);

    if (rank == 0) {
        if (sendtypes) {
             for (int i = 0; i < size; ++i) if (sendtypes[i] != MPI_DATATYPE_NULL && sendtypes[i] != MPI_BYTE) MPI_Type_free(&sendtypes[i]);
             free(sendtypes);
        }
         if (recvtypes) {
             for (int i = 0; i < size; ++i) if (recvtypes[i] != MPI_DATATYPE_NULL && recvtypes[i] != MPI_BYTE) MPI_Type_free(&recvtypes[i]);
             free(recvtypes);
         }
        free(sendcounts); free(displs); free(recvcounts); free(recvdispls);
    }

    // --- Rank 0 Finalization ---
    if (rank == 0) {
        // Finalize assumes results are correctly placed in img_result.img_pixels
        total_time = mpi_rank0_finalize_and_save(rank, size, start_time, &img, &img_result, args);
    }

    // --- Broadcast Time ---
    MPI_Bcast(&total_time, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    log_debug("Rank %d: Column processing finished. Returning time: %.6f", rank, total_time);
    return total_time;

col_cleanup_abort: // Label for cleanup before aborting
     free(my_input_col_pixels); free(my_output_col_pixels);
     if (column_recv_type != MPI_DATATYPE_NULL && column_recv_type != MPI_BYTE) MPI_Type_free(&column_recv_type);
     if (column_send_type != MPI_DATATYPE_NULL && column_send_type != MPI_BYTE) MPI_Type_free(&column_send_type);
     if (rank == 0) {
          if (sendtypes) {
              for (int i = 0; i < size; ++i) if (sendtypes[i] != MPI_DATATYPE_NULL && sendtypes[i] != MPI_BYTE) MPI_Type_free(&sendtypes[i]);
              free(sendtypes);
          }
          if (recvtypes) {
              for (int i = 0; i < size; ++i) if (recvtypes[i] != MPI_DATATYPE_NULL && recvtypes[i] != MPI_BYTE) MPI_Type_free(&recvtypes[i]);
              free(recvtypes);
          }
          free(sendcounts); free(displs); free(recvcounts); free(recvdispls);
          bmp_img_free(&img); bmp_img_free(&img_result); // Free images on abort
     }
     MPI_Abort(MPI_COMM_WORLD, 1);
     return -1.0;
}


