// SPDX-License-Identifier: GPL-3.0-or-later

#include "cli.h"
#include "../backend/compute-backend.h"
#include "args-parse.h"
#include "filters.h"
#include "modes.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

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

void cli_st_display_init(struct compute_backend *backend, struct p_args *args) {
	CHECK_DEBUG_MODE_IS_ON();

    gettimeofday(&start_time, NULL);

    cli_hide_cursor();

	char *input_files_str = args_get_filename_list_str(args);
	char *optional_modes_str = args_get_optional_modes_list_str(args);

    printf("\n");
    printf("  ┌─ convolution v1.2 ──────────────────────────┐\n");
    // Assuming args structure contains these fields. 
    // If these are globals, remove "args->".
    printf("  │ Input file    : %-28s│\n", input_files_str);
    printf("  │ Output file    : %-28s│\n", args->files_cfg.output_filename);
    printf("  │ Filter kernel        : %-28s│\n", filter_get_name(args->compute_cfg.filter_type)); 
    printf("  │ Mode          : %-28s│\n", backend->ops->get_name());
    printf("  │ Advanced-modes          : %-28s│\n", optional_modes_str);
    printf("  │                                             │\n");

    printf("  │ Status        : %-28s│\n", "Initializing...");
    printf("  │ Progress      : [....................]              │\n");
    printf("  │ Total Time    : 00:00.00                    │\n");
    printf("  │                                             │\n"); // Reserved line
    printf("  └─────────────────────────────────────────────┘\n");

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

    printf("  │ Status        : %-28s│\n", status);
    
    // Display loading bar without percentage
    printf("  │ Loading       : [%s]              │\n", color_bar);

    printf("  │ Total Time    : %-28s│\n", time_buf);

    // Move cursor back down 2 lines (over Reserve and Bottom Border)
    printf("\033[2B"); 

    fflush(stdout);
}

// Accepts computation_time (in seconds) to display separately from wall-clock time
void cli_st_display_finish(double computation_time) {
	CHECK_DEBUG_MODE_IS_ON();
    char total_time_buf[32];
    get_elapsed_str(total_time_buf);
    
    char comp_time_buf[32];
    int c_min = (int)computation_time / 60;
    int c_sec = (int)computation_time % 60;
    int c_mil = (int)((computation_time - (int)computation_time) * 100);
    sprintf(comp_time_buf, "%02d:%02d.%02d", c_min, c_sec, c_mil);

    // Move up to redraw the final state inside the box
    printf("\033[5A"); 
    
    printf("  │ Status        : %-28s│\n", "Completed");
    printf("  │ Progress      : [\033[32mDone                \033[0m]              │\n"); // Green 'Done'
    printf("  │ Total Time    : %-28s│\n", total_time_buf);
    
    // Use the reserved line for Real (Computation) Time
    printf("  │ Real Time     : %-28s│\n", comp_time_buf);
    
    // Redraw bottom border just in case
    printf("  └─────────────────────────────────────────────┘\n");

    cli_unhide_cursor();
    printf("\nDone.\n");
}
