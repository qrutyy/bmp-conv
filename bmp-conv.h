// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils/mt-utils.h"
#include <stdint.h>

#define LOG_FILE_PATH "tests/timing-results.dat"

struct p_args {
    int8_t threadnum;
    uint8_t block_size;
    char *input_filename;
    char *output_filename;
    char *filter_type;
    enum compute_mode mode;
};

