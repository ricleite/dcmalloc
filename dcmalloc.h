
#ifndef __DCMALLOC_H__
#define __DCMALLOC_H__

#define DCMALLOC_ATTR(s) __attribute__((s))
#define DCMALLOC_ALLOC_SIZE(s) DCMALLOC_ATTR(alloc_size(s))
#define DCMALLOC_EXPORT DCMALLOC_ATTR(visibility("default"))
#define DCMALLOC_NOTHROW DCMALLOC_ATTR(nothrow)

// called on process init/exit
void dc_malloc_initialize();
void dc_malloc_finalize();
// called on thread enter/exit
void dc_malloc_thread_initialize();
void dc_malloc_thread_finalize();

// exports
void* dc_malloc(size_t size)
    DCMALLOC_EXPORT DCMALLOC_NOTHROW DCMALLOC_ALLOC_SIZE(1);
void dc_free(void* ptr)
    DCMALLOC_EXPORT DCMALLOC_NOTHROW;

#endif // __DCMALLOC_H__
