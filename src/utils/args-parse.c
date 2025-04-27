// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include "args-parse.h"
#include "utils.h"
#include "../../logger/log.h"

const char *valid_filters[] = { "bb", "mb", "em", "gg", "gb", "co", "sh", "mm", "bo", "mg", NULL };

int parse_mandatory_args(int argc, char *argv[], struct p_args *args)
{
	for (int i = 1; i < argc; i++) {
		if (strncmp(argv[i], "--filter=", 9) == 0) {
			args->filter_type = check_filter_arg(argv[i] + 9);
			if (!args->filter_type)
				return -1;
			argv[i] = "_";
		} else if (strncmp(argv[i], "--mode=", 7) == 0) {
			args->compute_mode = check_mode_arg(argv[i] + 7);
			if (args->compute_mode < 0)
				return -1;
			argv[i] = "_";
		} else if (strncmp(argv[i], "--block=", 8) == 0) {
			args->block_size = atoi(argv[i] + 8);
			if (args->block_size <= 0) {
				log_error("Error: Block size must be > 0.\n");
				return -1;
			}
			argv[i] = "_"; // Mark as processed
		}
	}
	return 0;
}

int parse_queue_mode_args(int argc, char *argv[], struct p_args *args)
{
	uint8_t rww_found = 0;
	int wrt_temp = 0, ret_temp = 0, wot_temp = 0;
	char *rww_values = NULL;

	for (int i = 1; i < argc; i++) {
		if (strncmp(argv[i], "--log=", 6) == 0) {
			args->log_enabled = atoi(argv[i] + 6);
			argv[i] = "_";
		} else if (strncmp(argv[i], "--queue-size=", 13) == 0) {
			args->queue_capacity = (size_t)atoi(argv[i] + 13); // Store as bytes
			if (args->queue_capacity == 0 && strcmp(argv[i] + 13, "0") != 0) {
				log_error("Error: Invalid value for --queue-size.\n");
				return -1;
			}
			argv[i] = "_";
		} else if (strncmp(argv[i], "--queue-mem=", 12) == 0) {
			args->queue_memory_limit_mb = (size_t)atoi(argv[i] + 12);
			if (args->queue_memory_limit_mb == 0 && strcmp(argv[i] + 12, "0") != 0) {
				log_error("Error: Invalid value for --queue-mem.\n");
				return -1;
			}
			argv[i] = "_";
		} else if (strncmp(argv[i], "--output=", 9) == 0) {
			args->output_filename = argv[i] + 9;
			argv[i] = "_";
		} else if (strncmp(argv[i], "--rww=", 6) == 0) {
			rww_values = argv[i] + 6;
			if (sscanf(rww_values, "%d,%d,%d", &ret_temp, &wot_temp, &wrt_temp) != 3) {
				log_error("Error: Invalid format for --rww. Expected --rww=Readers,Workers,Writers.\n");
				return -1;
			}
			// Validate thread counts are within uint8_t range and positive
			if (wrt_temp <= 0 || wrt_temp > UCHAR_MAX || ret_temp <= 0 || ret_temp > UCHAR_MAX || wot_temp <= 0 || wot_temp > UCHAR_MAX) {
				log_error("Error: Thread counts in --rww must be between 1 and %d.\n", UCHAR_MAX);
				return -1;
			}
			args->wrt_count = (uint8_t)wrt_temp;
			args->ret_count = (uint8_t)ret_temp;
			args->wot_count = (uint8_t)wot_temp;
			rww_found = 1;
			argv[i] = "_";
		} else if (strncmp(argv[i], "_", 1) == 0) {
			continue;
		} else if (strncmp(argv[i], "-", 1) == 0) {
			log_error("Error: Unknown option in queue-mode: %s\n", argv[i]);
			return -1;
		} else if (args->file_count < DEFAULT_QUEUE_CAP) {
			// Assume remaining non-option args are input files
			args->input_filename[args->file_count++] = argv[i];
		} else {
			log_error("Error: Too many input files (max %d) or unknown argument: %s\n", argv[i]);
			return -1;
		}
	}

	if (!rww_found) {
		log_error("Error: Queue-based mode requires the --rww=R,W,T argument.\n");
		return -1;
	}
	if ((args->ret_count + args->wot_count + args->wrt_count) < 3) {
		log_error("Error: Queue-based mode requires at least 3 threads in total (1R, 1W, 1T).\n");
		return -1;
	}
	if (args->file_count == 0) {
		log_error("Error: Queue-based mode requires at least one input filename.\n");
		return -1;
	}

	return 0;
}

int parse_normal_mode_args(int argc, char *argv[], struct p_args *args)
{
	for (int i = 1; i < argc; i++) {
		if (strncmp(argv[i], "--threadnum=", 12) == 0) {
			args->threadnum = atoi(argv[i] + 12);
			if (args->threadnum <= 0) {
				log_error("Error: Thread count must be > 0.\n");
				return -1;
			}
			argv[i] = "_";
		} else if (strncmp(argv[i], "--log=", 6) == 0) {
			args->log_enabled = atoi(argv[i] + 6);
			argv[i] = "_";
		} else if (strncmp(argv[i], "--output=", 9) == 0) {
			args->output_filename = argv[i] + 9;
			argv[i] = "_";
		} else if (strncmp(argv[i], "_", 1) == 0) {
			continue; // Skip already processed args
		} else if (strncmp(argv[i], "-", 1) == 0) {
			log_error("Error: Unknown option in normal mode: %s\n", argv[i]);
			return -1;
		} else if (args->file_count == 0) {
			// Assume the first non-option, non-processed arg is the input file
			args->input_filename[0] = argv[i];
			args->file_count++;
		} else {
			// Found more than one potential input file or unknown arg
			log_error("Error: Normal mode accepts only one input file. Unknown argument: %s\n", argv[i]);
			return -1;
		}
	}

	if (args->file_count != 1) {
		log_error("Error: Normal (non-queued) mode requires exactly one input image filename.\n");
		return -1;
	}

	return 0;
}

void initialize_args(struct p_args *args_ptr)
{
	args_ptr->threadnum = 1;
	args_ptr->block_size = 0;
	args_ptr->output_filename = "";
	args_ptr->filter_type = NULL;
	args_ptr->compute_mode = -1;
	args_ptr->log_enabled = 0;
	args_ptr->mt_mode = 0;
	args_ptr->wrt_count = 0;
	args_ptr->ret_count = 0;
	args_ptr->wot_count = 0;
	args_ptr->file_count = 0;
	args_ptr->queue_memory_limit_mb = DEFAULT_QUEUE_MEM_LIMIT;
	args_ptr->queue_capacity = DEFAULT_QUEUE_CAP;

	for (int i = 0; i < DEFAULT_QUEUE_CAP; ++i) {
		args_ptr->input_filename[i] = NULL;
	}
}

char *check_filter_arg(char *filter)
{
	for (int i = 0; valid_filters[i] != NULL; i++) {
		if (strcmp(filter, valid_filters[i]) == 0) {
			return filter;
		}
	}
	log_error("Error: Invalid filter type '%s'. Valid types are: bb, mb, em, gg, gb, co, sh, mm, bo, mg\n", filter);
	return NULL;
}

int check_mode_arg(char *mode_str)
{
	for (int i = 0; valid_modes[i] != NULL; i++) {
		if (strcmp(mode_str, valid_modes[i]) == 0) {
			return i;
		}
	}
	log_error("Error: Invalid mode '%s'. Valid modes are: by_row, by_column, by_pixel, by_grid\n", mode_str);
	return -1;
}
