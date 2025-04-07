// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdlib.h>
#include <string.h>
#include "args-parse.h"
#include "utils.h"
#include <limits.h>

const char *valid_filters[] = { "bb", "mb", "em", "gg", "gb", "co", "sh", "mm", "bo", "mg", NULL };
const char *valid_modes[] = { "by_row", "by_column", "by_pixel", "by_grid", NULL };


int parse_mandatory_args(int argc, char *argv[], struct p_args *args) {
	for (int i = 2; i < argc; i++) {
		if (strncmp(argv[i], "--filter=", 9) == 0) {
			args->filter_type = check_filter_arg(argv[i] + 9);
			if (!args->filter_type) return -1;
			argv[i] = "_";
		} else if (strncmp(argv[i], "--mode=", 7) == 0) {
			args->compute_mode = check_mode_arg(argv[i] + 7);
			if (args->compute_mode < 0) return -1;
			argv[i] = "_";
		} else if (strncmp(argv[i], "--block=", 8) == 0) {
			args->block_size = atoi(argv[i] + 8);
			if (args->block_size <= 0) {
				fprintf(stderr, "Error: Block size must be >= 1.\n");
				return -1;
			}
			argv[i] = "_";
		}
	}
	return 0;
}

int parse_queue_mode_args(int argc, char *argv[], struct p_args *args) {
	uint8_t rww_found = 0;
	int wrt_temp = 0, ret_temp = 0, wot_temp = 0;
	char *rww_values = NULL;

	for (int i = 2; i < argc; i++) {
		if (strncmp(argv[i], "--log=", 6) == 0) {
			args->log_enabled = atoi(argv[i] + 6);
		} else if (strncmp(argv[i], "--lim=", 6) == 0) {
			args->queue_memory_limit = atoi(argv[i] + 6) * 1024 * 1024;
		} else if (strncmp(argv[i], "--output=", 9) == 0) {
			args->output_filename = argv[i] + 9;
		} else if (strncmp(argv[i], "--rww=", 6) == 0) {
			rww_values = argv[i] + 6;
			if (sscanf(rww_values, "%d,%d,%d", &ret_temp, &wot_temp, &wrt_temp) != 3) {
				fprintf(stderr, "Error: Invalid format for --rww. Expected --rww=W,R,T.\n");
				return -1;
			}
			if (wrt_temp <= 0 || wrt_temp > UCHAR_MAX || ret_temp <= 0 || ret_temp > UCHAR_MAX || wot_temp <= 0 || wot_temp > UCHAR_MAX) {
				fprintf(stderr, "Error: Thread counts in --rww must be between 1 and %d.\n", UCHAR_MAX);
				return -1;
			}
			args->wrt_count = wrt_temp;
			args->ret_count = ret_temp;
			args->wot_count = wot_temp;
			rww_found = 1;
			argv[i] = "_";
		} else if (strncmp(argv[i], "_", 1) == 0) {
			continue;
		} else if (args->file_count < MAX_IMAGE_QUEUE_SIZE) {
			args->input_filename[args->file_count++] = argv[i];
		} else {
			fprintf(stderr, "Error: Unknown argument %s\n", argv[i]);
			return -1;
		}
	}

	if (!rww_found) {
		fprintf(stderr, "Error: queue-based mode requires --rww=W,R,T argument.\n");
		return -1;
	}
	if ((args->ret_count + args->wot_count + args->wrt_count) < 3) {
		fprintf(stderr, "Error: queue-based mode requires at least 3 threads in total.\n");
		return -1;
	}

	return 0;
}

int parse_normal_mode_args(int argc, char *argv[], struct p_args *args) {
	for (int i = 1; i < argc; i++) {
		if (strncmp(argv[i], "--threadnum=", 12) == 0) {
			args->threadnum = atoi(argv[i] + 12);
			if (args->threadnum <= 0) {
				fprintf(stderr, "Error: Invalid threadnum.\n");
				return -1;
			}
		} else if (strncmp(argv[i], "--log=", 6) == 0) {
			args->log_enabled = atoi(argv[i] + 6);
		} else if (strncmp(argv[i], "--output=", 9) == 0) {
			args->output_filename = argv[i] + 9;
		} else if (args->input_filename[0] == NULL && strncmp(argv[i], "_", 1)) {
			args->input_filename[0] = argv[i];
			args->file_count++;
		} else if (strncmp(argv[i], "_", 1) == 0) {
			continue;
		} else {
			fprintf(stderr, "Error: Unknown argument %s\n", argv[i]);
			return -1;
		}
	}

	if (args->file_count != 1) {
		fprintf(stderr, "Error: non-queued mode requires exactly 1 input image.\n");
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
	args_ptr->queue_mode = 0;
	args_ptr->wrt_count = 0;
	args_ptr->ret_count = 0;
	args_ptr->wot_count = 0;
	args_ptr->file_count = 0;
	args_ptr->queue_memory_limit = 500 * 1024 * 1024;
	for (int i = 0; i < MAX_IMAGE_QUEUE_SIZE; ++i) {
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
	fputs("Error: Wrong filter.\n", stderr);
	return NULL;
}

int check_mode_arg(char *mode_str)
{
	for (int i = 0; valid_modes[i] != NULL; i++) {
		if (strcmp(mode_str, valid_modes[i]) == 0) {
			return i;
		}
	}
	fputs("Error: Invalid mode.\n", stderr);
	return -1;
}

const char *mode_to_str(int mode)
{
	if (mode >= 0 && (unsigned long)mode < (sizeof(valid_modes) / sizeof(valid_modes[0]) - 1)) {
		return valid_modes[mode];
	}
	if (mode == -1) {
		return "unset/invalid";
	}
	return "unknown";
}
