// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../utils/args-parse.h"
#include "../utils/filters.h"

/**
 * Every mode (ST, MT, QMT, MPI) implements this interface 
 */
struct pr_mode_ops {
	int (*init)(struct pr_mode *mode, struct p_args *args, struct filter_mix *filters);
	
	double (*run_processing)(struct pr_mode *mode);
	
	void (*cleanup)(struct pr_mode *mode);
	
	const char *(*get_name)(void);
};

/**
 * Basic mode structure 
 */
struct pr_mode {
	const struct pr_mode_ops *ops;
	void *mode_data; // ctx data 
	struct p_args *args;
	struct filter_mix *filters;
};

struct pr_mode *pr_mode_create(struct p_args *args);

void pr_mode_destroy(struct pr_mode *mode);
