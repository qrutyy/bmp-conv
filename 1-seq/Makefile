WFLAGS := -Wall -Wpedantic -Wextra 
CFLAGS := $(WFLAGS) -g -lm

INPUT_TEST?=image2.bmp
FILTER_TYPE?=mb

seq-con: seq-con.c
	$(CC) -o seq-con seq-con.c ../libbmp/libbmp.c ../utils/utils.c $(CFLAGS)

run-seq-con:
	./seq-con $(INPUT_TEST) $(FILTER_TYPE) 

clean:
	rm -rf *.out main *.o

.PHONY:
	clean run_main
