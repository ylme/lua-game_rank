
CC = gcc
CFLAGS = -g -std=gnu11 -fPIC -O2 -Wall -Wextra

binary_rank: binary_rank.o
	$(CC) -o $@.so -shared -llua $^

clean:
	rm -f binary_rank.o
	rm -f binary_rank.so
    
binary_rank.o: binary_rank.c
