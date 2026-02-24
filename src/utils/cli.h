#ifndef CLI_H
#define CLI_H

#include "args-parse.h"
#include "../backend/compute-backend.h"

void cli_st_display_init(struct p_args *args, struct compute_backend *backend);

void cli_st_display_update(const char *status);

void cli_st_display_finish(double computation_time);

#endif
