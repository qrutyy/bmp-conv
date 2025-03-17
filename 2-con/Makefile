WFLAGS := -Wall -Wpedantic -Wextra 
CFLAGS := $(WFLAGS) -g -lm

INPUT_TEST?=image2.bmp
FILTER_TYPE?=mb
THREAD_NUM?=2
COMPUTE_MODE=by_column

main: main.c
	$(CC) -o main main.c ../libbmp/libbmp.c ../utils/utils.c $(CFLAGS)
	make run_main

run_main:
	./main $(INPUT_TEST) $(FILTER_TYPE) --threadnum=$(THREAD_NUM) --mode=$(COMPUTE_MODE)

clean:
	rm -rf *.out main *.o

.PHONY:
	clean run_main
