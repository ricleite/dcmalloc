/*
 * Copyright (C) 2019 Ricardo Leite. All rights reserved.
 * Licenced under the MIT licence. See COPYING file in the project root for details.
 */

#ifndef __STATIC_MALLOC_H__
#define __STATIC_MALLOC_H__

#include <stddef.h>
#include <stdbool.h>

// static_malloc interface
// goal here is to make some small memory allocations from static memory
void* static_calloc(size_t n, size_t size);
// returns tree if ptr was allocated by static_allocator
bool try_static_free(void* ptr);

#endif // __STATIC_MALLOC_H__

