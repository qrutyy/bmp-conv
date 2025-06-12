// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdlib.h>
#include <stdint.h>

#define DEFAULT_QUEUE_CAP 20
#define DEFAULT_QUEUE_MEM_LIMIT 500

// Structure for storing input arguments. Better described in README
struct p_args {
	uint8_t block_size;
	char *input_filename[DEFAULT_QUEUE_CAP];
	char *output_filename;
	uint8_t file_count;
	char *filter_type;
	int8_t compute_mode;
	uint8_t log_enabled;
	uint8_t mt_mode; // 0 - non-queue-mode, 1 - queue-mode, 2 - MPI mode
	int8_t threadnum;
	uint8_t wrt_count; // writer threads count
	uint8_t ret_count; // reader threads count
	uint8_t wot_count; // worker threads count
	uint32_t queue_capacity; // max el count in queue
	size_t queue_memory_limit_mb;
};

/**
 * Initializes the p_args structure with default values before parsing.
 * Sets counts to 0 or 1, pointers to NULL or empty strings, and modes/flags to sensible defaults.
 *
 * @param args_ptr Pointer to the p_args structure to initialize.
 */
void initialize_args(struct p_args *args_ptr);

/**
 * Checks if the provided mode string is present in the list of valid modes.
 *
 * @param mode_str The mode string extracted from the command line argument.
 * 
 * @return The integer index corresponding to the mode if valid, -1 otherwise.
 */
int check_mode_arg(char *mode_str);

/**
 * Checks if the provided filter string is present in the list of valid filters.
 *
 * @param filter The filter string extracted from the command line argument.
 * 
 * @return The original filter string pointer if valid, NULL otherwise.
 */
char *check_filter_arg(char *filter);

/**
 * Parses mandatory arguments shared by both normal and queue modes:
 * --filter=<type>, --mode=<mode>, --block=<size>.
 * Validates the arguments and stores them in the args structure. Replaces processed argument strings in argv with "_" to mark them as handled.
 *
 * @param argc Argument count from main().
 * @param argv Argument vector from main().
 * @param args Pointer to the p_args structure to store parsed values.
 * 
 * @return 0 on success, -1 on parsing or validation error.
 */
int parse_mandatory_args(int argc, char *argv[], struct p_args *args);

/**
 * Parses arguments specific to the queue-based execution mode:
 * --log=<0|1>, --lim=<MB>, --output=<prefix>, --rww=<R,W,T>, and input filenames.
 * Validates the --rww argument format and range. Collects remaining non-option arguments as input filenames. Marks processed arguments in argv with "_".
 *
 * @param argc Argument count from main().
 * @param argv Argument vector from main().
 * @param args Pointer to the p_args structure to store parsed values.
 * 
 * @return 0 on success, -1 on parsing or validation error.
 */
int parse_queue_mode_args(int argc, char *argv[], struct p_args *args);

/**
 * Parses arguments specific to the normal (non-queue) execution mode:
 * --threadnum=<N>, --log=<0|1>, --output=<file>, and the single input filename.
 * Validates the thread number. Expects exactly one input filename. Marks processed arguments in argv with "_".
 *
 * @param argc Argument count from main().
 * @param argv Argument vector from main().
 * @param args Pointer to the p_args structure to store parsed values.
 * 
 * @return 0 on success, -1 on parsing or validation error.
 */
int parse_normal_mode_args(int argc, char *argv[], struct p_args *args);
