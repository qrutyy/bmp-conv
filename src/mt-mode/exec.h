// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../libbmp/libbmp.h"
#include "../utils/threads-general.h"

#pragma once

double execute_mt_computation(int threadnum, struct img_dim *dim, struct img_spec *img_spec, struct p_args *args, struct filter_mix *filters);
