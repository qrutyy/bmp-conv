// SPDX-License-Identifier: GPL-3.0-or-later

#include "mt-queue.h"
#include "mt-utils.h"
#include "utils.h"
#include <pthread.h>
#include <string.h>
#include <stdatomic.h>
#include <sys/time.h>
#include <errno.h>

// TODO: add better logging + test

/** Notes about balanced distributed work:
  * 1. Its kinda already balanced when the size of the queue is limited.
  * 2. We can calculate the ammount of used per thread memory/pixels and limit by it (so it would fair balanced)... i was thinking that thats stupid, but i guess thats the only good idea i have by now. 
  * 3. Memory usage per thread isn't a good metric in terms of metrics that directly depend on execution time. However, at first i won't depend on threadnum, block_size and other metrics that affect the execution time.
  * 4. There is an idea to block the reader, but thats already was made in queue_push/queue_pop in both sides.
*/

#define RAW_MEM_OVERHEAD (512 * 1024) // some auxiliary data structures and all this stuff...
#define NSEC_OFFSET (1000 * 1000000) // 100 ms in nanoseconds

// due to queue_pop sleep mechanism - we should use global atomic for writers
size_t written_files = 0;
// almost the same with read_files
size_t read_files = 0;

pthread_barrier_t reader_barrier;

// yeah, its not that precise. baytik tuda syuda...
static size_t estimate_image_memory(const bmp_img *img)
{
	size_t bytes_per_pixel, pixel_data_size, bmp_struct_size, row_pointers_size = 0;

	bytes_per_pixel = img->img_header.biBitCount / 8;
	if (bytes_per_pixel == 0)
		bytes_per_pixel = 1;

	pixel_data_size = (size_t)img->img_header.biWidth * img->img_header.biHeight * bytes_per_pixel;
	bmp_struct_size = sizeof(bmp_img);
	row_pointers_size = (size_t)img->img_header.biHeight * sizeof(bmp_pixel *);

	return pixel_data_size + row_pointers_size + bmp_struct_size + RAW_MEM_OVERHEAD;
}

void queue_init(struct img_queue *q, size_t max_mem)
{
	q->front = q->rear = q->size = 0;
	q->current_mem_usage = 0;
	q->max_mem_usage = max_mem;
	if (q->max_mem_usage == 0) {
		q->max_mem_usage = MAX_QUEUE_MEMORY;
	}
	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->cond_non_empty, NULL);
	pthread_cond_init(&q->cond_non_full, NULL);
}

static void queue_push(struct img_queue *q, bmp_img *img, char *filename)
{
	struct queue_img_info *iq_info = NULL;
	size_t image_memory = 0;
	double start_block_time = 0;
	double result_time = 0;

	if (!filename)
		return;
	iq_info = malloc(sizeof(struct queue_img_info));
	if (!iq_info) {
		fprintf(stderr, "Error: memory allocation failed in 'queue_push'\n");
		exit(-1); // panic button
	}
	iq_info->filename = filename;
	iq_info->image = img;

	pthread_mutex_lock(&q->mutex);

	image_memory = estimate_image_memory(img);

	while (q->size >= MAX_QUEUE_SIZE || (q->current_mem_usage + image_memory > q->max_mem_usage && q->size > 0)) {
		start_block_time = get_time_in_seconds();
		pthread_cond_wait(&q->cond_non_full,&q->mutex); 
		// unblocks threads with other roles (f.e. workers, that require some elements for convolution) 
		// and waits for return cond_non_full signal
	}

	result_time = (start_block_time != 0) ? get_time_in_seconds() - start_block_time : 0;

	q->images[q->rear] = iq_info;
	q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
	q->size++;
	q->current_mem_usage += image_memory;

	if (result_time)
		qt_write_logs(result_time, QPUSH);

	pthread_cond_signal(&q->cond_non_empty);
	pthread_mutex_unlock(&q->mutex);
}

static bmp_img *queue_pop(struct img_queue *q, char **filename, uint8_t file_count)
{
	struct queue_img_info *iqi = NULL;
	bmp_img *img_src = NULL;
	size_t image_memory = 0;
	double start_block_time = 0;
	double result_time = 0;
	struct timespec wait_time;
	int wait_result = 0;

	pthread_mutex_lock(&q->mutex);

restart_loop:

	while (q->size == 0) {// waiting for some images to pop from queue
		start_block_time = get_time_in_seconds();
		if (__atomic_load_n(&written_files, __ATOMIC_ACQUIRE) >= file_count)
			return NULL;
		
		clock_gettime(CLOCK_REALTIME, &wait_time);
        wait_time.tv_nsec += NSEC_OFFSET;
        wait_time.tv_sec += wait_time.tv_nsec / 1000000000;
        wait_time.tv_nsec %= 1000000000;
		wait_result = pthread_cond_timedwait(&q->cond_non_empty, &q->mutex, &wait_time);

		if (wait_result == ETIMEDOUT) {
             printf("Consumer timed out.\n");
            if (__atomic_load_n(&written_files, __ATOMIC_ACQUIRE) >= file_count) {
                goto exit_overload; 
			}
			goto restart_loop;
        } else if (wait_result == 0) {
             printf("Consumer signalled.\n");
            if (__atomic_load_n(&written_files, __ATOMIC_ACQUIRE) >= file_count) {
                goto exit_overload;
            }
	    } else {
            perror("pthread_cond_timedwait error");
            pthread_mutex_unlock(&q->mutex);
            goto exit_overload;
        }
	}

	result_time = (start_block_time != 0) ? get_time_in_seconds() - start_block_time : 0;
	
	iqi = q->images[q->front];
	image_memory = estimate_image_memory(iqi->image);

	q->front = (q->front + 1) % MAX_QUEUE_SIZE;
	q->size--;
	q->current_mem_usage -= image_memory;

	if (iqi->filename) {
		// NOLINTBEGIN
		*filename = strdup(iqi->filename); // strdup = malloc + strcpy
		// NOLINTEND
		if (!*filename) {
			fprintf(stderr, "Error: strdup failed in queue_pop\n");
		}
	}

	img_src = iqi->image;
	free(iqi);

	if (result_time)
		qt_write_logs(result_time, QPOP);

	pthread_cond_signal(&q->cond_non_full);
	pthread_mutex_unlock(&q->mutex);
	return img_src;

exit_overload:
	pthread_cond_signal(&q->cond_non_full);
	pthread_mutex_unlock(&q->mutex);
	return NULL;
}

// just reads the image from the queue. doesn't set up img-specific structures
void *reader_thread(void *arg)
{
	struct qthreads_info *qt_info = (struct qthreads_info *)arg;
	size_t i = 0;
	bmp_img *img;
	char filepath[MAX_PATH_LEN];
	bmp_img *empty_img = NULL;
	double start_time = 0;
	double result_time = 0;
	size_t read_files_local = 0;
	
	while (1) {
		read_files_local = __atomic_fetch_add(&read_files, 1, __ATOMIC_ACQUIRE);
		if (read_files_local >= qt_info->pargs->file_count)
			break;

		start_time = get_time_in_seconds();

		img = malloc(sizeof(bmp_img));
		if (!img) {
			fprintf(stderr, "Reader Error: Failed to allocate bmp_img struct\n");
			break;
		}
		snprintf(filepath, sizeof(filepath), "test-img/%s", qt_info->pargs->input_filename[read_files_local]);
		if (bmp_img_read(img, filepath)) {
			fprintf(stderr, "Reader Error: Could not open %s\n", qt_info->pargs->input_filename[read_files_local]);
			exit(-1); // panic button
		}
		queue_push(qt_info->input_q, img, qt_info->pargs->input_filename[read_files_local]);

		// future work(termination signals) is made more for integrity and better threads work,
		// so ending time measuring here. 
		result_time = get_time_in_seconds() - start_time;
		if (result_time)
			qt_write_logs(result_time, READER);
		
		printf("Reader: Read %s\n", filepath);
	}
	
	pthread_barrier_wait(&reader_barrier); // waiting for pushing every file to queue (to avoid early termination signal)

	for (i = 0; i < (size_t)(qt_info->pargs->wot_count / qt_info->pargs->ret_count + 2); i++) {
		empty_img = calloc(1, sizeof(bmp_img));
		if (empty_img) {
			queue_push(qt_info->input_q, empty_img, NULL);
		} else {
			fprintf(stderr, "Reader Error: Failed to allocate termination signal\n");
		}
		queue_push(qt_info->input_q, empty_img, NULL);
	}
	printf("Reader: Finished reading files and sent termination signals.\n");
	return NULL;
}

void *worker_thread(void *arg)
{
	struct qthreads_info *qt_info = (struct qthreads_info *)arg;
	uint16_t next_x_block_local, next_y_block_local, process_result = 0;
	pthread_mutex_t local_xy_mutex = PTHREAD_MUTEX_INITIALIZER;
	bmp_img *img = NULL;
	bmp_img *img_result = NULL;
	struct thread_spec *th_spec = NULL;
	struct img_dim *dim = NULL;
	struct img_spec *img_spec = NULL;
	char *filename = NULL;
	double start_time = 0;
	double result_time = 0;

	while (1) {
		// even though allocation are time consuming - for comparison in this case we need raw thread time
		start_time = get_time_in_seconds();

		img = queue_pop(qt_info->input_q, &filename, qt_info->pargs->file_count);
		if (!img) {
			// determines signal receivement.
			fprintf(stderr, "Worker Error: queue_pop returned NULL image pointer\n");
			if (filename)
				free(filename);
			break;
		}

		if (img->img_header.biWidth == 0 && img->img_header.biHeight == 0) {
			printf("Worker: Received termination signal.\n");
			break;
		}

		printf("Worker: Popped image (w:%d, %d)\n", img->img_header.biWidth, img->img_header.biHeight);

		img_result = malloc(sizeof(bmp_img));
		if (!img_result) {
			fprintf(stderr, "Worker Error: Result image allocation failed\n");
			bmp_img_free(img);
			continue;
		}
		bmp_img_init_df(img_result, img->img_header.biWidth, img->img_header.biHeight);

		th_spec = thread_spec_init();
		if (!th_spec) {
			fprintf(stderr, "Worker Error: thread_spec allocation failed\n");
			free(img_result);
			bmp_img_free(img);
			continue;
		}

		dim = init_dimensions(img->img_header.biWidth, img->img_header.biHeight);
		if (!dim) {
			fprintf(stderr, "Worker Error: init_dimensions failed\n");
			free(th_spec);
			free(img_result);
			bmp_img_free(img);
			continue;
		}

		img_spec = init_img_spec(img, img_result);
		if (!img_spec) {
			fprintf(stderr, "Worker Error: init_img_spec failed\n");
			free(dim);
			free(th_spec);
			free(img_result);
			bmp_img_free(img);
			continue;
		}

		th_spec->dim = dim;
		th_spec->img = img_spec;
		next_x_block_local = 0;
		next_y_block_local = 0;

		while (1) {
			process_result = 0;
			// split to the mt
			switch ((enum compute_mode)qt_info->pargs->compute_mode) {
			case BY_ROW:
				process_result = process_by_row(th_spec, &next_x_block_local, qt_info->pargs->block_size, &local_xy_mutex);
				break;
			case BY_COLUMN:
				process_result = process_by_column(th_spec, &next_y_block_local, qt_info->pargs->block_size, &local_xy_mutex);
				break;
			case BY_PIXEL:
				process_result = process_by_pixel(th_spec, &next_x_block_local, &next_y_block_local, &local_xy_mutex);
				break;
			case BY_GRID:
				process_result = process_by_grid(th_spec, &next_x_block_local, &next_y_block_local, qt_info->pargs->block_size, &local_xy_mutex);
				break;
			default:
				fprintf(stderr, "Worker Error: Invalid mode %d\n", qt_info->pargs->compute_mode);
				process_result = -1;
				break;
			}

			if (process_result != 0) {
				pthread_mutex_destroy(&local_xy_mutex);
				break;
			}
			filter_part_computation(th_spec, qt_info->pargs->filter_type, qt_info->filters);
		}
		queue_push(qt_info->output_q, th_spec->img->output_img, filename);
		
		result_time = get_time_in_seconds() - start_time;
		if (result_time)
			qt_write_logs(result_time, WORKER);

		printf("Worker Pushed result to output queue.\n");

		bmp_img_free(img);
		free(img);
		free(dim);
		free(th_spec);
	}
	printf("Worker exiting.\n");
	pthread_mutex_destroy(&local_xy_mutex);
	return NULL;
}

void *writer_thread(void *arg)
{
	struct qthreads_info *qt_info = (struct qthreads_info *)arg;
	char output_filepath[MAX_PATH_LEN];
	bmp_img *img = NULL;
	char *filename = malloc(sizeof(char));
	double start_time = 0;
	double result_time = 0;
	ssize_t current_wf = 0;

	printf("File count: %d\n", qt_info->pargs->file_count);

	while (1) {
		start_time = get_time_in_seconds();

		if (__atomic_load_n(&written_files, __ATOMIC_ACQUIRE) >= qt_info->pargs->file_count)
			break;
		
		img = queue_pop(qt_info->output_q, &filename, qt_info->pargs->file_count);
		if (!img)
			break;

		if (img->img_header.biWidth == 0 && img->img_header.biHeight == 0) {
			break;
		}

		if (strlen(qt_info->pargs->output_filename))
			snprintf(output_filepath, sizeof(output_filepath), "test-img/q_out_%s_%s", qt_info->pargs->output_filename, filename);
		else
			snprintf(output_filepath, sizeof(output_filepath), "test-img/q_out_%s", filename);

		bmp_img_write(img, output_filepath);
		
		result_time = get_time_in_seconds() - start_time;
		if (result_time)
			qt_write_logs(result_time, WRITER);

		bmp_img_free(img);
		current_wf = __atomic_add_fetch(&written_files, 1, __ATOMIC_ACQUIRE);
		printf("Successfully write %s file with index %zu\n", output_filepath, current_wf);
	}
	pthread_cond_signal(&qt_info->output_q->cond_non_empty); // sending signal in case some writer thread is waiting in queue_pop
	return NULL;
}

int allocate_qthread_resources(struct qthreads_info *qt_info, struct p_args *args_ptr, struct img_queue *input_queue, struct img_queue *output_queue)
{
	size_t q_mem_limit = 0;

	qt_info->ret_info = malloc(sizeof(struct threads_info));
	qt_info->wrt_info = malloc(sizeof(struct threads_info));
	qt_info->wot_info = malloc(sizeof(struct threads_info));

	qt_info->wot_info->threads = malloc(args_ptr->wot_count * sizeof(pthread_t));
	if (!qt_info->wot_info->threads && args_ptr->wot_count > 0) {
		goto mem_err;
	}

	qt_info->ret_info->threads = malloc(args_ptr->ret_count * sizeof(pthread_t));
	if (!qt_info->ret_info->threads && args_ptr->ret_count > 0) {
		free(qt_info->ret_info->threads);
		qt_info->ret_info->threads = NULL;
		goto mem_err;
	}

	qt_info->wrt_info->threads = malloc(args_ptr->wrt_count * sizeof(pthread_t));
	if (!qt_info->wrt_info->threads && args_ptr->wrt_count > 0) {
		free(qt_info->ret_info->threads);
		qt_info->ret_info->threads = NULL;
		free(qt_info->wot_info->threads);
		qt_info->wot_info->threads = NULL;
		goto mem_err;
	}

	q_mem_limit = args_ptr->queue_memory_limit > 0 ? args_ptr->queue_memory_limit : MAX_QUEUE_MEMORY;

	queue_init(input_queue, q_mem_limit);
	queue_init(output_queue, q_mem_limit);

	qt_info->pargs = args_ptr;
	qt_info->input_q = input_queue;
	qt_info->output_q = output_queue;

	return 0;

mem_err:
	fprintf(stderr, "Error: memory allocation failed at allocate_qthread_resources\n");
	return -1;
}

// yes, too much args, but at what cost ;)
void create_qthreads(struct qthreads_info *qt_info, struct p_args *args_ptr)
{
	size_t i;
	printf("Creating %hhu readers, %hhu workers, %hhu writers\n", args_ptr->ret_count, args_ptr->wot_count, args_ptr->wrt_count);

	pthread_barrier_init(&reader_barrier, NULL, qt_info->pargs->ret_count);

	for (i = 0; i < args_ptr->ret_count; i++) {
		if (pthread_create(&qt_info->ret_info->threads[i], NULL, reader_thread, qt_info)) {
			perror("Failed to create a reader thread");
			break;
		}
		qt_info->ret_info->used_threads++;
	}

	for (i = 0; i < args_ptr->wot_count; i++) {
		if (pthread_create(&qt_info->wot_info->threads[i], NULL, worker_thread, qt_info)) {
			perror("Failed to create a worker thread");
			break;
		}
		qt_info->wot_info->used_threads++;
	}

	for (i = 0; i < args_ptr->wrt_count; i++) {
		if (pthread_create(&qt_info->wrt_info->threads[i], NULL, writer_thread, qt_info)) {
			perror("Failed to create a writer thread");
			break;
		}
		qt_info->wrt_info->used_threads++;
	}
}

void join_qthreads(struct qthreads_info *qt_info)
{
	size_t i;
	for (i = 0; i < qt_info->ret_info->used_threads; i++) {
		if (pthread_join(qt_info->ret_info->threads[i], NULL)) {
			fprintf(stderr, "Failed to join a reader thread");
		}
	}

	for (i = 0; i < qt_info->wot_info->used_threads; i++) {
		if (pthread_join(qt_info->wot_info->threads[i], NULL)) {
			fprintf(stderr, "Failed to join a worker thread");
		}
	}

	for (i = 0; i < qt_info->wrt_info->used_threads; i++) {
		if (pthread_join(qt_info->wrt_info->threads[i], NULL)) {
			fprintf(stderr, "Failed to join a writer thread");
		}
	}
	pthread_barrier_destroy(&reader_barrier);
}

void free_qthread_resources(struct qthreads_info *qt_info)
{
	free(qt_info->wot_info->threads);
	free(qt_info->ret_info->threads);
	free(qt_info->wrt_info->threads);
	free(qt_info->wot_info);
	free(qt_info->ret_info);
	free(qt_info->wrt_info);
	free(qt_info);
}
