
CC=gcc
CFLAGS=-shared -fPIC -std=c11 -O2 -Wall
LDFLAGS=-ldl -pthread

default: dcmalloc.so

dcmalloc.so: dcmalloc.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o dcmalloc.so dcmalloc.c

clean:
	rm -f *.so
