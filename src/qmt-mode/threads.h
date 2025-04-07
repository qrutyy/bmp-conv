
#pragma once 

#define MAX_PATH_LEN 40

void *reader_thread(void *arg);
void *writer_thread(void *arg);
void *worker_thread(void *arg);

