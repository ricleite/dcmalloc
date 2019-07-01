#
# Copyright (C) 2019 Ricardo Leite. All rights reserved.
# Licenced under the MIT licence. See COPYING file in the project root for details.
#

CC=gcc
DFLAGS=-ggdb -g -fno-omit-frame-pointer
CFLAGS=-shared -fPIC -std=c11 -O2 -Wall $(DFLAGS)
LDFLAGS=-ldl -pthread $(DFLAGS)

default: dcmalloc.so

dcmalloc.so: dcmalloc.c static_malloc.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o dcmalloc.so dcmalloc.c static_malloc.c

clean:
	rm -f *.so
