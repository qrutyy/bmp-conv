// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "utils/threads-general.h"

typedef struct thread_spec wi_spec;

double opencl_execute_basic_computation(struct img_spec *img_spec, struct p_args *args, struct filter_mix *filters);
