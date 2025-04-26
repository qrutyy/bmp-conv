// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdlib.h>
#include <stdint.h>

#define DEFAULT_QUEUE_CAP 20
#define DEFAULT_QUEUE_MEM_LIMIT 500
#pragma once

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

void initialize_args(struct p_args *args_ptr);

int check_mode_arg(char *mode_str);
char *check_filter_arg(char *filter);

int parse_mandatory_args(int argc, char *argv[], struct p_args *args);
int parse_queue_mode_args(int argc, char *argv[], struct p_args *args);
int parse_normal_mode_args(int argc, char *argv[], struct p_args *args);
