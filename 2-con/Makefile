WFLAGS := -Wall -Wpedantic -Wextra 
CFLAGS := $(WFLAGS) -g -lm

INPUT_TEST?=image2.bmp
FILTER_TYPE?=mb
THREAD_NUM?=2
COMPUTE_MODE=by_column
BLOCK_SIZE=5

mt-non-cn: mt_non_con.c
	$(CC) -o mt-non-con mt-non-con.c ../libbmp/libbmp.c ../utils/utils.c mt-utils.c $(CFLAGS)

run-non-con:
	./mt-non-con $(INPUT_TEST) $(FILTER_TYPE) --threadnum=$(THREAD_NUM) --mode=$(COMPUTE_MODE)

mt-con: mt-con.c
	$(CC) -o mt-con mt-con.c ../libbmp/libbmp.c ../utils/utils.c mt-utils.c  $(CFLAGS)

run-con:
	./mt-con $(INPUT_TEST) $(FILTER_TYPE) --threadnum=$(THREAD_NUM) --mode=$(COMPUTE_MODE) --block=$(BLOCK_SIZE)

clean:
	rm -rf *.out mt-con mt-non-con *.o tests/*.dat

.PHONY:
	clean run_non_con run_con
