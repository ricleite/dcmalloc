/*
 * Copyright (C) 2019 Ricardo Leite. All rights reserved.
 * Licenced under the MIT licence. See COPYING file in the project root for details.
 */

#include <assert.h>
#include <stdio.h>

#include "static_malloc.h"

// use up to 1MB for static storage
#define STATIC_SIZE     0x100000

// static storage pool
static char mem_pool[STATIC_SIZE];
static size_t mem_idx = 0;

void* static_calloc(size_t n, size_t size)
{
    size_t true_size = n * size;
    void* ptr = (void*)&mem_pool[mem_idx];

    mem_idx += true_size;
    if (mem_idx >= STATIC_SIZE)
        assert(false);

    return ptr;
}

bool try_static_free(void* ptr)
{
    // no actual memory management, so no actual "freeing" of memory
    return ptr >= (void*)mem_pool && ptr <= (void*)mem_pool;
}

