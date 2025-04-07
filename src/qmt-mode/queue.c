#include "queue.h"
#include "../utils/utils.h"
#include <pthread.h>
#include <string.h>
#include <stdatomic.h>

#define RAW_MEM_OVERHEAD (512 * 1024) // some auxiliary data structures and all this stuff...

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

void queue_push(struct img_queue *q, bmp_img *img, char *filename)
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

bmp_img *queue_pop(struct img_queue *q, char **filename, uint8_t file_count)
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

		set_wait_time(&wait_time);	
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

exit_overload:
	pthread_cond_signal(&q->cond_non_full);
	pthread_mutex_unlock(&q->mutex);
	return img_src;
}

void qt_write_logs(double result_time, enum LOG_TAG tag)
{
	FILE *file = NULL;

	file = fopen(QT_LOG_FILE_PATH, "a");
	const char *log_tag_str = log_tag_to_str(tag);

	if (file) {
		fprintf(file, "%s %.6f\n", log_tag_str, result_time);
		fclose(file);
	} else {
		fputs("Error: could not open timing results file for appending.\n", stderr);
	}

	return;
}
