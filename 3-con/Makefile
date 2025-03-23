WFLAGS := -Wall -Wpedantic -Wextra 
CFLAGS := $(WFLAGS) -g -lm

INPUT_TEST1?=image1.bmp
INPUT_TEST2?=image2.bmp
FILTER_TYPE?=mb
THREAD_NUM?=2
COMPUTE_MODE=by_column
BLOCK_SIZE=5

UTILS_PATH = ../utils
LIBBMP_PATH = ../libbmp

UTILS_SRC = $(UTILS_PATH)/utils.c $(UTILS_PATH)/mt-utils.c
LIBBMP_SRC = $(LIBBMP_PATH)/libbmp.c

mt-piped-con: mt-piped-con.c
	$(CC) -o mt-piped-con mt-piped-con.c $(LIBBMP_SRC) $(UTILS_SRC) $(CFLAGS)

run-piped-con:
	./mt-piped-con $(INPUT_TEST1) $(INPUT_TEST2) $(FILTER_TYPE) --threadnum=$(THREAD_NUM) --mode=$(COMPUTE_MODE) --block=$(BLOCK_SIZE)

clean:
	rm -rf *.out mt-con *.o tests/*.dat

.PHONY:
	clean run_non_con run_con
