// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../../libbmp/libbmp.h"
#include "../utils/threads-general.h"

// mmmm its intuitively clear, too late to add doc
double execute_mt_computation(int threadnum, struct img_dim *dim, struct img_spec *img_spec, struct p_args *args, struct filter_mix *filters);
