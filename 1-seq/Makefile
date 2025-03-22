WFLAGS := -Wall -Wpedantic -Wextra 
CFLAGS := $(WFLAGS) -g -lm

INPUT_TEST?=image2.bmp
FILTER_TYPE?=mb

UTILS_PATH = ../utils
LIBBMP_PATH = ../libbmp

UTILS_SRC = $(UTILS_PATH)/utils.c
LIBBMP_SRC = $(LIBBMP_PATH)/libbmp.c

seq-con: seq-con.c
	$(CC) -o seq-con seq-con.c $(LIBBMP_SRC) $(UTILS_SRC) $(CFLAGS)

run-seq-con:
	./seq-con $(INPUT_TEST) $(FILTER_TYPE) 

clean:
	rm -rf *.out main *.o

.PHONY:
	clean run_main
