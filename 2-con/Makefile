WFLAGS := -Wall -Wpedantic -Wextra 
CFLAGS := $(WFLAGS) -g -lm

INPUT_TEST?=image2.bmp
FILTER_TYPE?=mb
THREAD_NUM?=2
COMPUTE_MODE=by_column
BLOCK_SIZE=5

UTILS_PATH = ../utils
LIBBMP_PATH = ../libbmp

UTILS_SRC = $(UTILS_PATH)/utils.c $(UTILS_PATH)/mt-utils.c
LIBBMP_SRC = $(LIBBMP_PATH)/libbmp.c

mt-con: mt-con.c
	$(CC) -o mt-con mt-con.c $(LIBBMP_SRC) $(UTILS_SRC) $(CFLAGS)

run-con:
	./mt-con $(INPUT_TEST) $(FILTER_TYPE) --threadnum=$(THREAD_NUM) --mode=$(COMPUTE_MODE) --block=$(BLOCK_SIZE)

clean:
	rm -rf *.out mt-con *.o tests/*.dat

.PHONY:
	clean run_non_con run_con
