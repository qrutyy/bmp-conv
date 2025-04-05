WFLAGS := -Wall -Wpedantic -Wextra 
CFLAGS := $(WFLAGS) -lm -O3

INPUT_TF?=image2.bmp
FILTER_TYPE?=mb
THREAD_NUM?=1
COMPUTE_MODE?=by_column
BLOCK_SIZE?=5
OUTPUT_FILE?=""
LOG?=1
RWW_MIX?=1,1,1

UTILS_PATH = src/utils
LIBBMP_PATH = libbmp

UTILS_SRC = $(UTILS_PATH)/utils.c $(UTILS_PATH)/mt-utils.c $(UTILS_PATH)/mt-queue.c
LIBBMP_SRC = $(LIBBMP_PATH)/libbmp.c

build: src/bmp-conv.c
	$(CC) -o src/bmp-conv src/bmp-conv.c $(LIBBMP_SRC) $(UTILS_SRC) $(CFLAGS)

run:
	./src/bmp-conv $(INPUT_TF) --filter=$(FILTER_TYPE) --threadnum=$(THREAD_NUM) --mode=$(COMPUTE_MODE) --block=$(BLOCK_SIZE) --output=$(OUTPUT_FILE) --log=$(LOG)

run-mac-e-cores:
#\ This task will be executed with minimal prioriy and on E-cores. Used for background tasks, which should not interfere with the user’s work.
	taskpolicy -c background ./src/bmp-conv $(INPUT_TF) --filter=$(FILTER_TYPE) --threadnum=$(THREAD_NUM) --mode=$(COMPUTE_MODE) --block=$(BLOCK_SIZE) --log=$(LOG) --output=$(OUTPUT_FILE)

run-mac-p-cores:
#\ By default (taskpolicy without -c), macOS automatically allocates the process to all available kernels, both P-cores and E-cores. I've noticed that it has the same performance as in taskset's 'utility' mode. There is no available way to lock process execution only to P-cores, its kinda wrongly.
	make run

run-q-mode:
	./src/bmp-conv -queue-mode $(INPUT_TF) --mode=$(COMPUTE_MODE) --filter=$(FILTER_TYPE) --block=$(BLOCK_SIZE) --rww=$(RWW_MIX)

clean:
	rm -rf src/*.out src/bmp-conv src/*.o tests/*.dat 

.PHONY:
	clean run
