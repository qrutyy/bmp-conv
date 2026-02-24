#ifndef CLI_H
#define CLI_H

#include "args-parse.h"
#include "../backend/compute-backend.h"

#ifdef DEBUG_MODE
#define CHECK_DEBUG_MODE_IS_ON() do {} while (0);
#else 
#define CHECK_DEBUG_MODE_IS_ON() return;
#endif

void cli_st_display_init(struct compute_backend *backend, struct p_args *args);

void cli_st_display_update(const char *status);

void cli_st_display_finish(double computation_time);

#endif
