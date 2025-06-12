// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../../libbmp/libbmp.h"
#include "../utils/args-parse.h"
#include "../utils/utils.h"
#include "threads.h"
#include "queue.h"

/**
 * Allocates memory for thread management structures (reader, worker, writer thread arrays)
 * and initializes the input and output image queues based on program arguments.
 * Handles potential memory allocation failures gracefully by cleaning up partially
 * allocated resources.
 *
 * yeah, its not that precise. baytik tuda syuda...
 *
 * @param qt_info A pointer to the main qthreads_gen_info structure to be populated.
 * @param args_ptr A pointer to the parsed program arguments containing thread counts and queue memory limits.
 * @param input_queue A pointer to the img_queue structure for input images.
 * @param output_queue A pointer to the img_queue structure for processed images.
 * @return 0 on success, -1 on memory allocation failure.
 */
int allocate_qthread_resources(struct qthreads_gen_info *qt_info, struct p_args *args_ptr, struct img_queue *input_queue, struct img_queue *output_queue);

/**
 * Waits for all created reader, worker, and writer threads to complete execution
 * by joining them. Destroys the reader synchronization barrier after readers finish.
 *
 * @param qt_info A pointer to the qthreads_gen_info structure containing the thread IDs and the count of actually created threads.
 * @return void. Errors during join are logged.
 */
void join_qthreads(struct qthreads_gen_info *qt_info);

/**
 * Creates and launches reader, worker, and writer threads based on the counts
 * specified in program arguments. Initializes a barrier for reader synchronization.
 * Stores thread IDs in the provided qt_info structure. Handles thread creation errors.
 *
 * @param qt_info A pointer to the initialized qthreads_gen_info structure containing thread arrays, arguments, queues, and the barrier.
 * @param args_ptr A pointer to the parsed program arguments (potentially redundant).
 * @return void. Errors during thread creation are logged.
 */
void create_qthreads(struct qthreads_gen_info *qt_info);

/**
 * Frees all memory allocated for thread management structures, including the
 * arrays holding thread IDs and the main qt_info structure itself.
 * Assumes queues are handled/freed elsewhere if necessary.
 *
 * @param qt_info A pointer to the qthreads_gen_info structure whose resources are to be freed. Can be NULL.
 * @return void.
 */
void free_qthread_resources(struct qthreads_gen_info *qt_info);
