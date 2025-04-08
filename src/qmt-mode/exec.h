// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../libbmp/libbmp.h"
#include "../utils/args-parse.h"
#include "../utils/utils.h"
#include "queue.h"

int allocate_qthread_resources(struct qthreads_gen_info *qt_info, struct p_args *args_ptr, struct img_queue *input_queue, struct img_queue *output_queue);
void join_qthreads(struct qthreads_gen_info *qt_info);
void create_qthreads(struct qthreads_gen_info *qt_info);
void free_qthread_resources(struct qthreads_gen_info *qt_info);

void st_write_logs(struct p_args *args, double result_time);
void qt_write_logs(double result_time, enum LOG_TAG tag);
