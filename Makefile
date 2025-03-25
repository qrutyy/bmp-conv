WFLAGS := -Wall -Wpedantic -Wextra 
CFLAGS := $(WFLAGS) -lm -O3

INPUT_TF?=image2.bmp
FILTER_TYPE?=mb
THREAD_NUM?=1
COMPUTE_MODE?=by_column
BLOCK_SIZE?=5
OUTPUT_FILE?=""
LOG?=1

UTILS_PATH = utils
LIBBMP_PATH = libbmp

UTILS_SRC = $(UTILS_PATH)/utils.c $(UTILS_PATH)/mt-utils.c
LIBBMP_SRC = $(LIBBMP_PATH)/libbmp.c

build: bmp-conv.c
	$(CC) -o bmp-conv bmp-conv.c $(LIBBMP_SRC) $(UTILS_SRC) $(CFLAGS)

run:
	./bmp-conv $(INPUT_TF) --filter=$(FILTER_TYPE) --threadnum=$(THREAD_NUM) --mode=$(COMPUTE_MODE) --block=$(BLOCK_SIZE) --output=$(OUTPUT_FILE) --log=$(LOG)

run-e-cores:
#\ This task will be executed with minimal prioriy and on E-cores. Used for background tasks, which should not interfere with the user’s work.
	taskpolicy -c background ./bmp-conv $(INPUT_TF) --filter=$(FILTER_TYPE) --threadnum=$(THREAD_NUM) --mode=$(COMPUTE_MODE) --block=$(BLOCK_SIZE) --log=$(LOG) --output=$(OUTPUT_FILE)

run-p-cores:
#\ By default (taskpolicy without -c), macOS automatically allocates the process to all available kernels, both P-cores and E-cores. I've noticed that it has the same performance as in taskset's utility mode. There is no available way to lock process execution only to P-cores.
	make run

clean:
	rm -rf *.out bmp-conv *.o tests/*.dat 

.PHONY:
	clean run
