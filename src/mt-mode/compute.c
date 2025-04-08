#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "../../logger/log.h"
#include "../utils/threads-general.h"
#include "compute.h"

uint8_t process_by_row(struct thread_spec *th_spec, uint16_t *next_x_block, uint16_t block_size, pthread_mutex_t *x_block_mutex)
{
	pthread_mutex_lock(x_block_mutex);
	log_debug("next_block: %u, height: %d\n", *next_x_block, th_spec->dim->height);

	if (*next_x_block >= th_spec->dim->height) {
		pthread_mutex_unlock(x_block_mutex);
		th_spec->start_row = th_spec->end_row = 0;
		return 1;
	}
	th_spec->start_row = *next_x_block;
	*next_x_block += block_size;
	pthread_mutex_unlock(x_block_mutex);

	th_spec->start_column = 0;
	th_spec->end_row = fmin(th_spec->start_row + block_size, th_spec->dim->height);
	th_spec->end_column = th_spec->dim->width;

	return 0;
}

uint8_t process_by_column(struct thread_spec *th_spec, uint16_t *next_y_block, uint16_t block_size, pthread_mutex_t *y_block_mutex)
{
	pthread_mutex_lock(y_block_mutex);
	log_debug("next_block: %d, width: %d\n", *next_y_block, th_spec->dim->width);

	if (*next_y_block >= th_spec->dim->width) {
		pthread_mutex_unlock(y_block_mutex);
		th_spec->start_column = th_spec->end_column = 0;
		return 1;
	}

	th_spec->start_column = *next_y_block;
	*next_y_block += block_size;
	pthread_mutex_unlock(y_block_mutex);

	th_spec->start_row = 0;
	th_spec->end_row = th_spec->dim->height;
	th_spec->end_column = fmin(th_spec->start_column + block_size, th_spec->dim->width);

	return 0;
}

uint8_t process_by_grid(struct thread_spec *th_spec, uint16_t *next_x_block, uint16_t *next_y_block, uint16_t block_size, pthread_mutex_t *xy_block_mutex)
{
	pthread_mutex_lock(xy_block_mutex);

	if (*next_x_block >= th_spec->dim->height || *next_y_block >= th_spec->dim->width) {
		pthread_mutex_unlock(xy_block_mutex);
		th_spec->start_row = th_spec->end_row = 0;
		th_spec->start_column = th_spec->end_column = 0;
		return 1;
	}

	th_spec->start_row = *next_x_block;
	th_spec->start_column = *next_y_block;
	*next_y_block += block_size;

	if (*next_y_block >= th_spec->dim->width) {
		*next_y_block = 0;
		*next_x_block += block_size;
	}
	pthread_mutex_unlock(xy_block_mutex);

	th_spec->end_row = fmin(th_spec->start_row + block_size, th_spec->dim->height);
	th_spec->end_column = fmin(th_spec->start_column + block_size, th_spec->dim->width);
	log_debug("Row: st: %d, end: %d, Column: st: %d, end: %d \n", th_spec->start_row, th_spec->end_row, th_spec->start_column, th_spec->end_column);

	return 0;
}

uint8_t process_by_pixel(struct thread_spec *th_spec, uint16_t *next_x_block, uint16_t *next_y_block, pthread_mutex_t *xy_block_mutex)
{
	return process_by_grid(th_spec, next_x_block, next_y_block, 1, xy_block_mutex);
}
