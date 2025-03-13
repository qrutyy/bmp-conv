WFLAGS := -Wall -Wpedantic -Wextra 
CFLAGS := $(WFLAGS) -g -lm

INPUT_TEST?=image2.bmp
FILTER_TYPE?=mb

main:
	$(CC) -o main main.c libbmp/libbmp.c $(CFLAGS)
	./main $(INPUT_TEST) $(FILTER_TYPE)

clean:
	rm -rf *.bmp *.out main main2

.PHONY:
	clean
