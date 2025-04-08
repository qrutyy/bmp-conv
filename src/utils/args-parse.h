// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdlib.h>
#include <stdint.h>

#define MAX_IMAGE_QUEUE_SIZE 20
#define QUEUE_MEM_LIMIT (500 * 1024 * 1024)
#pragma once 

// Structure for storing input arguments. Better described in README
struct p_args {
	uint8_t block_size;
	char *input_filename[MAX_IMAGE_QUEUE_SIZE];
	char *output_filename;
	uint8_t file_count;
	char *filter_type;
	int8_t compute_mode;
	uint8_t log_enabled;
	uint8_t queue_mode;
	int8_t threadnum;
	uint8_t wrt_count; // writer threads count
	uint8_t ret_count; // reader threads count
	uint8_t wot_count; // worker threads count
	size_t queue_memory_limit;
};

void initialize_args(struct p_args *args_ptr);

int check_mode_arg(char *mode_str);
char *check_filter_arg(char *filter);

int parse_mandatory_args(int argc, char *argv[], struct p_args *args);
int parse_queue_mode_args(int argc, char *argv[], struct p_args *args); 
int parse_normal_mode_args(int argc, char *argv[], struct p_args *args); 

