// SPDX-License-Identifier: GPL-3.0-or-later

#include "cli.h"
#include "../backend/compute-backend.h"
#include "args-parse.h"
#include "filters.h"
#include "modes.h"
#include "../logger/log.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

#define MAX_CLI_ERRORS 20
#define MAX_ERROR_LEN 256

static struct {
    char errors[MAX_CLI_ERRORS][MAX_ERROR_LEN];
    int count;
} cli_errors = { .count = 0 };

static struct p_args *stored_args = NULL;
static struct compute_backend *stored_backend = NULL;

static void cli_log_callback(log_Event *ev) {
    if (ev->level >= LOG_ERROR && cli_errors.count < MAX_CLI_ERRORS) {
        vsnprintf(cli_errors.errors[cli_errors.count], MAX_ERROR_LEN, ev->fmt, ev->ap);
        cli_errors.count++;
    }
}

static struct timeval start_time;

static void cli_hide_cursor(void)
{
    printf("\033[?25l");
}

static void cli_unhide_cursor(void)
{
    printf("\033[?25h");
}

static void get_elapsed_str(char* buffer) {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    double elapsed = (current_time.tv_sec - start_time.tv_sec) + 
                     (current_time.tv_usec - start_time.tv_usec) / 1000000.0;

    int minutes = (int)elapsed / 60;
    int seconds = (int)elapsed % 60;
    int millis = (int)((elapsed - (int)elapsed) * 100);
    sprintf(buffer, "%02d:%02d.%02d", minutes, seconds, millis);
}

void cli_st_display_init(struct p_args *args, struct compute_backend *backend) {
	CHECK_DEBUG_MODE_IS_ON();

    stored_args = args;
    stored_backend = backend;
    log_add_callback(cli_log_callback, NULL, LOG_ERROR);

    gettimeofday(&start_time, NULL);

    cli_hide_cursor();

	char *input_files_str = args_get_filename_list_str(args);
	char *optional_modes_str = args_get_optional_modes_list_str(args);

    char threads_str[64];
    if (args->compute_cfg.queue == CONV_QUEUE_ENABLED) {
         snprintf(threads_str, sizeof(threads_str), "R:%d W:%d T:%d", 
            args->compute_ctx.qm.threads_cfg.reader_cnt,
            args->compute_ctx.qm.threads_cfg.writer_cnt,
            args->compute_ctx.qm.threads_cfg.worker_cnt);
    } else {
         snprintf(threads_str, sizeof(threads_str), "%d", args->compute_ctx.threadnum > 0 ? args->compute_ctx.threadnum : 1);
    }

    printf("\n");
    printf("  ┌─ convolution v0.2 ──────────────────────────────┐\n");
    // Assuming args structure contains these fields. 
    // If these are globals, remove "args->".
    printf("  │ %-18s: %-28s│\n", "Input file", input_files_str);
    printf("  │ %-18s: %-28s│\n", "Output file", args->files_cfg.output_filename);
    printf("  │ %-18s: %-28s│\n", "Filter kernel", filter_get_name(args->compute_cfg.filter_type)); 
    printf("  │ %-18s: %-28s│\n", "Mode", backend->ops->get_name());
    printf("  │ %-18s: %-28s│\n", "Threads", threads_str);
    printf("  │ %-18s: %-28s│\n", "Advanced-modes", optional_modes_str);
    printf("  │                                                 │\n");

    printf("  │ %-18s: %-28s│\n", "Status", "Initializing...");
    printf("  │ %-18s: [....................]      │\n", "Progress");
    printf("  │ %-18s: 00:00.00                    │\n", "Total Time");
    printf("  │                                                 │\n"); // Reserved line
    printf("  └─────────────────────────────────────────────────┘\n");

	free(input_files_str);
	free(optional_modes_str);
}

void cli_st_display_update(const char *status) {
	CHECK_DEBUG_MODE_IS_ON();

    char time_buf[32];
    get_elapsed_str(time_buf);

    // Animation state
    static int anim_pos = 0;
    static int anim_dir = 1;
    const int bar_width = 20;
    const int block_width = 3;
    char bar[128] = "";

    // Calculate bouncing animation
    // Clear bar buffer with spaces
    for (int i = 0; i < bar_width; i++) {
        bar[i] = ' ';
    }
    bar[bar_width] = '\0';

    // Draw the moving block "<=>"
    for (int i = 0; i < block_width; i++) {
        int pos = anim_pos + i;
        if (pos >= 0 && pos < bar_width) {
            if (i == 0) bar[pos] = '<';
            else if (i == block_width - 1) bar[pos] = '>';
            else bar[pos] = '=';
        }
    }

    // Update position for next call
    anim_pos += anim_dir;
    if (anim_pos >= (bar_width - block_width) || anim_pos <= 0) {
        anim_dir *= -1;
    }

    // Format with colors
    char color_bar[256];
    sprintf(color_bar, "\033[36m%s\033[0m", bar); // Cyan color for loader

    // Move 5 lines up (to Status line)
    // 1: Bottom Border, 2: Reserve, 3: Time, 4: Progress, 5: Status
    printf("\033[5A"); 

    printf("  │ %-18s: %-28s│\n", "Status", status);
    
    // Display loading bar without percentage
    printf("  │ %-18s: [%s]%-6s│\n", "Loading", color_bar, "");

    printf("  │ %-18s: %-28s│\n", "Total Time", time_buf);

    // Move cursor back down 2 lines (over Reserve and Bottom Border)
    printf("\033[2B"); 
    
    fflush(stdout);
}

// Accepts computation_time (in seconds) to display separately from wall-clock time
void cli_st_display_finish(double computation_time) {
	CHECK_DEBUG_MODE_IS_ON();
    char total_time_buf[32];
	char *optional_modes_str = args_get_optional_modes_list_str(stored_args);
    get_elapsed_str(total_time_buf);

    char threads_str[64];
    if (stored_args->compute_cfg.queue == CONV_QUEUE_ENABLED) {
         snprintf(threads_str, sizeof(threads_str), "R:%d W:%d T:%d", 
            stored_args->compute_ctx.qm.threads_cfg.reader_cnt,
            stored_args->compute_ctx.qm.threads_cfg.writer_cnt,
            stored_args->compute_ctx.qm.threads_cfg.worker_cnt);
    } else {
         snprintf(threads_str, sizeof(threads_str), "%d", stored_args->compute_ctx.threadnum > 0 ? stored_args->compute_ctx.threadnum : 1);
    }
    
    char comp_time_buf[32];
    int c_min = (int)computation_time / 60;
    int c_sec = (int)computation_time % 60;
    int c_mil = (int)((computation_time - (int)computation_time) * 100);
    sprintf(comp_time_buf, "%02d:%02d.%02d", c_min, c_sec, c_mil);

    // Move up to redraw output filename and statuses the box
    printf("\033[11A"); 
    
    printf("  │ %-18s: %-28s│\n", "Output file", stored_args->files_cfg.output_filename);
    printf("  │ %-18s: %-28s│\n", "Filter kernel", filter_get_name(stored_args->compute_cfg.filter_type)); 
    printf("  │ %-18s: %-28s│\n", "Mode", stored_backend->ops->get_name());
    printf("  │ %-18s: %-28s│\n", "Threads", threads_str);
    printf("  │ %-18s: %-28s│\n", "Advanced-modes", optional_modes_str);
    printf("  │                                                 │\n");

	if (computation_time) {
		printf("  │ %-18s: %-28s│\n", "Status", "Completed");
		printf("  │ %-18s: [\033[32mDone                \033[0m]      │\n", "Progress"); // Green 'Done'
		printf("  │ %-18s: %-28s│\n", "Total Time", total_time_buf);
		printf("  │ %-18s: %-28s│\n", "Real Time", comp_time_buf);
	} else {
		printf("  │ %-18s: %-28s│\n", "Status", "Failed");
        printf("  │ %-18s: [\033[31mFailed              \033[0m]      │\n", "Progress");
        printf("  │ %-18s: %-28s│\n", "Total Time", total_time_buf);
        printf("  │                                                 │\n");
	}

    // Redraw bottom border just in case
    printf("  └─────────────────────────────────────────────────┘\n");

    cli_unhide_cursor();

    if (cli_errors.count > 0) {
        printf("\n  \033[31mCRITICAL ERRORS:\033[0m\n");
        for (int i = 0; i < cli_errors.count; i++) {
            printf("  - %s\n", cli_errors.errors[i]);
        }
        printf("\n");
    }
}
