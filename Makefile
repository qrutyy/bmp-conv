WFLAGS := -Wall -Wpedantic -Wextra 

CFLAGS := $(WFLAGS) -g -lm


main:
	$(CC) -o main main.c libbmp/libbmp.c $(CFLAGS)

main2:
	$(CC) -o main2 main2.c cbmp/cbmp.c $(CFLAGS) 

clean:
	rm -rf *.bmp *.out main main2

.PHONY:
	clean
