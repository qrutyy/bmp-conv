// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdlib.h>
#include <stdint.h>

#define DEFAULT_QUEUE_CAP 20
#define DEFAULT_QUEUE_MEM_LIMIT 500

struct files_cfg {
	char **input_filename;
	char *output_filename;
	uint8_t file_cnt;
};

struct threads_cfg {
	uint8_t writer_cnt; // writer threads cnt
	uint8_t reader_cnt; // reader threads cnt
	uint8_t worker_cnt; // worker threads cnt
};

struct compute_cfg {
	char *filter_type;
	uint8_t block_size;

	int8_t compute_mode : 2;
	// 0 - non-queue-mode, 1 - queue-mode, 2 - MPI mode
	uint8_t mt_mode : 2; 
};

// Structure for storing input arguments. Better described in README
struct p_args {
	struct files_cfg files_cfg;
	struct compute_cfg compute_cfg;
	
	union {
		struct {
			struct threads_cfg threads_cfg;
			uint32_t tq_capacity; // max el cnt in queue
			size_t tq_memory_limit_mb;
		} qm;
		int8_t threadnum;
	} mt_mode_cfg; 

	uint8_t log_enabled : 1;

};

extern const char *valid_filters[];

/**
 * Initializes the p_args structure with default values before parsing.
 * Sets cnts to 0 or 1, pointers to NULL or empty strings, and modes/flags to sensible defaults.
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
 * @param argc Argument cnt from main().
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
 * @param argc Argument cnt from main().
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
 * @param argc Argument cnt from main().
 * @param argv Argument vector from main().
 * @param args Pointer to the p_args structure to store parsed values.
 *
 * @return 0 on success, -1 on parsing or validation error.
 */
int parse_normal_mode_args(int argc, char *argv[], struct p_args *args);
