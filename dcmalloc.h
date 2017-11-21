
#ifndef __DCMALLOC_H__
#define __DCMALLOC_H__

#define DCMALLOC_ATTR(s) __attribute__((s))
#define DCMALLOC_ALLOC_SIZE(s) DCMALLOC_ATTR(alloc_size(s))
#define DCMALLOC_ALLOC_SIZE2(s1, s2) DCMALLOC_ATTR(alloc_size(s1, s2))
#define DCMALLOC_EXPORT DCMALLOC_ATTR(visibility("default"))
#define DCMALLOC_NOTHROW DCMALLOC_ATTR(nothrow)

#define dc_malloc malloc
#define dc_free free
#define dc_calloc calloc
#define dc_realloc realloc
#define dc_posix_memalign posix_memalign
#define dc_aligned_alloc aligned_alloc
#define dc_valloc valloc
#define dc_memalign memalign
#define dc_pvalloc pvalloc

// called on process init/exit
void dc_malloc_initialize();
void dc_malloc_finalize();
// called on thread enter/exit
void dc_malloc_thread_initialize();
void dc_malloc_thread_finalize();

// exports
// malloc interface
void* dc_malloc(size_t size)
    DCMALLOC_EXPORT DCMALLOC_NOTHROW DCMALLOC_ALLOC_SIZE(1);
void dc_free(void* ptr)
    DCMALLOC_EXPORT DCMALLOC_NOTHROW;
void* dc_calloc(size_t n, size_t size)
    DCMALLOC_EXPORT DCMALLOC_NOTHROW DCMALLOC_ALLOC_SIZE2(1, 2);
void* dc_realloc(void* ptr, size_t size)
    DCMALLOC_EXPORT DCMALLOC_NOTHROW DCMALLOC_ALLOC_SIZE(2);
// memory alignment ops
int dc_posix_memalign(void** memptr, size_t alignment, size_t size)
    DCMALLOC_EXPORT DCMALLOC_NOTHROW DCMALLOC_ATTR(nonnull(1))
    DCMALLOC_ALLOC_SIZE(3);
void* dc_aligned_alloc(size_t alignment, size_t size)
    DCMALLOC_EXPORT DCMALLOC_NOTHROW DCMALLOC_ALLOC_SIZE(2);
void* dc_valloc(size_t size)
    DCMALLOC_EXPORT DCMALLOC_NOTHROW DCMALLOC_ALLOC_SIZE(1);
// obsolete alignment oos
void* dc_memalign(size_t alignment, size_t size)
    DCMALLOC_EXPORT DCMALLOC_NOTHROW DCMALLOC_ALLOC_SIZE(2);
void* dc_pvalloc(size_t size)
    DCMALLOC_EXPORT DCMALLOC_NOTHROW DCMALLOC_ALLOC_SIZE(1);

#endif // __DCMALLOC_H__
