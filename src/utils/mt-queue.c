// SPDX-License-Identifier: GPL-3.0-or-later

#include "mt-queue.h"
#include "mt-utils.h"
#include <pthread.h>
#include <string.h>

// TODO: add better logging + test

void queue_init(struct img_queue *q)
{
	q->front = q->rear = q->size = 0;
	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->cond, NULL);
}

static void queue_push(struct img_queue *q, bmp_img *img, char *filename)
{
	struct queue_img_info *iq_info = NULL;

	iq_info = malloc(sizeof(struct queue_img_info));
	if (!iq_info) {
		fprintf(stderr, "Error: memory allocation failed in 'queue_push'\n");
		exit(-1); // panic button
	}
	iq_info->filename = filename;
	iq_info->image = img;

	pthread_mutex_lock(&q->mutex);

	while (q->size == MAX_QUEUE_SIZE)
		pthread_cond_wait(&q->cond, &q->mutex);

	q->images[q->rear] = iq_info;
	q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
	q->size++;

	pthread_cond_signal(&q->cond);
	pthread_mutex_unlock(&q->mutex);
}

static bmp_img *queue_pop(struct img_queue *q, char **filename)
{
	struct queue_img_info *iqi = NULL;
	bmp_img *img_src = NULL;

	pthread_mutex_lock(&q->mutex);

	while (q->size == 0)
		pthread_cond_wait(&q->cond, &q->mutex);

	iqi = q->images[q->front];
	q->front = (q->front + 1) % MAX_QUEUE_SIZE;
	q->size--;
	
	if (iqi->filename) {
        *filename = strdup(iqi->filename); // strdup = malloc + strcpy
        if (!*filename) {
            fprintf(stderr, "Error: strdup failed in queue_pop\n");
        }
    }
	
	img_src = iqi->image;
	free(iqi);

	pthread_cond_signal(&q->cond);
	pthread_mutex_unlock(&q->mutex);
	return img_src;
}

// just reads the image from the queue. doesn't set up img-specific structures
void *reader_thread(void *arg)
{
	struct qthreads_info *qt_info = (struct qthreads_info *)arg;
	size_t i = 0;
	bmp_img *img;
	char filepath[MAX_PATH_LEN];
	bmp_img *empty_img  = NULL;



	for (i = 0; i < qt_info->pargs->file_count; i++) {
		img = malloc(sizeof(bmp_img));
		if (!img) {
			fprintf(stderr, "Reader Error: Failed to allocate bmp_img struct\n");
			continue; 
		}
		snprintf(filepath, sizeof(filepath), "test-img/%s", qt_info->pargs->input_filename[i]);
		if (bmp_img_read(img, filepath)) {
			fprintf(stderr, "Reader Error: Could not open %s\n", qt_info->pargs->input_filename[i]);
			exit(-1); // panic button
		}
		queue_push(qt_info->input_q, img, qt_info->pargs->input_filename[i]);
		printf("Reader: Read %s\n", filepath);
	}
	for (i = 0; i < qt_info->pargs->wot_count; i++) {
		empty_img = calloc(1, sizeof(bmp_img));
        if (empty_img) {
            queue_push(qt_info->input_q, empty_img, NULL); // Передаем указатель на пустую структуру
        } else {
            fprintf(stderr, "Reader Error: Failed to allocate termination signal\n");
        }
		queue_push(qt_info->input_q, empty_img, "");
	}
	printf("Reader: Finished reading files and sent termination signals.\n");
	return NULL;
}

void *worker_thread(void *arg)
{
	struct qthreads_info *qt_info = (struct qthreads_info *)arg;
	printf("Worker started.\n");

	uint16_t next_x_block_local, next_y_block_local, process_result = 0;
	pthread_mutex_t local_xy_mutex = PTHREAD_MUTEX_INITIALIZER;
	bmp_img *img = NULL;
	bmp_img *img_result = NULL;
	struct thread_spec *th_spec = NULL;
	struct img_dim *dim = NULL;
	struct img_spec *img_spec = NULL;
	char *filename = NULL;

	while (1) {
		img = queue_pop(qt_info->input_q, &filename);
		if (!img) {
            fprintf(stderr, "Worker Error: queue_pop returned NULL image pointer\n");
            if (filename) free(filename); // Освобождаем имя файла, если оно было получено
            continue;
		}
		printf("height %d, width %d \n", img->img_header.biHeight, img->img_header.biWidth);
		
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
				if (process_result < 0) {
					fprintf(stderr, "Worker Error: process_by_* failed.\n");
				}
				break;
			}
			filter_part_computation(th_spec, qt_info->pargs->filter_type, qt_info->filters);
		}
		queue_push(qt_info->output_q, th_spec->img->output_img, filename);
		printf("Worker Pushed result to output queue.\n");
	
		free(img);
		free(dim);
		free(th_spec);
		bmp_img_free(img);

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
	size_t i = 0;
	char *filename = malloc(sizeof(char));

	printf("File count: %d\n", qt_info->pargs->file_count);

	for (i = 0; i < qt_info->pargs->file_count; i++) {
		img = queue_pop(qt_info->output_q, &filename);
		if (img->img_header.biWidth == 0 && img->img_header.biHeight == 0) {
			break;
		}
		printf("%s\n", qt_info->pargs->output_filename);
		if (strlen(qt_info->pargs->output_filename))
			snprintf(output_filepath, sizeof(output_filepath), "test-img/q_out_%s_%s", qt_info->pargs->output_filename, filename);
		else 
			snprintf(output_filepath, sizeof(output_filepath), "test-img/q_out_%s", filename);

		bmp_img_write(img, output_filepath); // getting input image at output
		bmp_img_free(img);

		printf("Successfully write %s\n", output_filepath);
	
		free(img);
	}
	return NULL;
}

int allocate_qthread_resources(struct qthreads_info *qt_info, struct p_args *args_ptr, struct img_queue *input_queue, struct img_queue *output_queue)
{
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

	queue_init(input_queue);
	queue_init(output_queue);

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

	for (i = 0; i < qt_info->wrt_info->used_threads; i++) {
		if (pthread_join(qt_info->wrt_info->threads[i], NULL)) {
			perror("Failed to join a writer thread");
		}
	}

	for (i = 0; i < qt_info->wot_info->used_threads; i++) {
		if (pthread_join(qt_info->wot_info->threads[i], NULL)) {
			perror("Failed to join a worker thread");
		}
	}

	for (i = 0; i < qt_info->ret_info->used_threads; i++) {
		if (pthread_join(qt_info->ret_info->threads[i], NULL)) {
			perror("Failed to join a reader thread");
		}
	}
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
