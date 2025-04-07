// SPDX-License-Identifier: GPL-3.0-or-later

#include "../../libbmp/libbmp.h"
#include "../utils/args-parse.h"
#include "../utils/utils.h"
#include "queue.h"
#include <pthread.h>
#include <stdint.h>


// thread-work specific struct for better abstraction (#saynotoglobals)
struct threads_info {
	uint8_t used_threads;
	pthread_t *threads;
};

// all the information needed while thread execution in queue-mode
struct qthreads_gen_info {
	struct threads_info *wot_info;
	struct threads_info *ret_info;
	struct threads_info *wrt_info;
	struct p_args *pargs;
	struct filter_mix *filters;
	struct img_queue *output_q;
	struct img_queue *input_q;
};

int allocate_qthread_resources(struct qthreads_gen_info *qt_info, struct p_args *args_ptr, struct img_queue *input_queue, struct img_queue *output_queue);
void join_qthreads(struct qthreads_gen_info *qt_info);
void create_qthreads(struct qthreads_gen_info *qt_info, struct p_args *args_ptr);
void free_qthread_resources(struct qthreads_gen_info *qt_info);

void st_write_logs(struct p_args *args, double result_time);
void qt_write_logs(double result_time, enum LOG_TAG tag);

