#include <stdlib.h>
#include "../../libbmp/libbmp.h"
#include "threads.h"


size_t written_files = 0;
size_t read_files = 0;

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
	size_t current_wf = 0;

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

