
#define _GNU_SOURCE
#include <dlfcn.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdatomic.h>

#include <unistd.h>

#include <pthread.h>

#include "defines.h"
#include "static_malloc.h"
#include "dcmalloc.h"

#define max(a,b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
       _a > _b ? _a : _b; })

#define min(a,b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
       _a > _b ? _b : _a; })

/*
 * Begin dcmalloc compile time options
 */

// print debug-level messages for every malloc hook
// #define DCMALLOC_DEBUG
// collect thread allocation behavior
// this requires keeping metadata per allocation (i.e each malloc will require more memory)
#define DCMALLOC_COLLECT_THREAD_BEHAVIOR

/*
 * End dcmalloc compile time options
 */

#ifdef DCMALLOC_DEBUG
#define PRINT_DEBUG(STR, ...) \
    fprintf(stdout, "%s:%d %s " STR "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__);

#else
#define PRINT_DEBUG(str, ...)

#endif

#define PRINT_ERR(STR, ...) \
    fprintf(stderr, "%s:%d %s " STR "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)

/*
 * Begin dc_malloc global data
 */

#define MAX_ALLOC 100000
#define DCMALLOC_OUTPUT_FILE ".dcmalloc.dat"


// each thread tracks allocation data in thread-local storage
// and then synchronizes it to global data on thread exit
// can only track exact allocation size up to a number
// larger allocations are counted on the largest histogram value

// thread local data
// number of allocations per size
static _Thread_local size_t alloc_sizes[MAX_ALLOC];
// number of allocations per size with no cross-thread free
static _Thread_local size_t alloc_sizes_thread[MAX_ALLOC];

// global data
static size_t global_alloc_sizes[MAX_ALLOC];
static size_t global_alloc_sizes_thread[MAX_ALLOC];
static pthread_mutex_t global_alloc_mutex = PTHREAD_MUTEX_INITIALIZER;
static int initialized = 0;

// used when tracking per-allocation data
// need to ensure sizeof(alloc_data) is a multiple of word size
// https://stackoverflow.com/questions/11130109/c-struct-size-alignment 
struct alloc_data;

struct alloc_data
{
    pthread_t tid;
    size_t size;
    // need space for at least a ptr placed before allocation
    struct alloc_data* pad;
} DCMALLOC_ATTR(aligned(sizeof(void*)));

typedef struct alloc_data alloc_data;

/*
 * Begin dc_malloc functions.
 */

void dc_malloc_initialize()
{
    if (initialized)
        return;

    initialized = 1;
    PRINT_DEBUG();
    if (atexit(dc_malloc_finalize) != 0)
        PRINT_ERR("cannot set exit function");
}

void dc_malloc_finalize()
{
    if (!initialized)
        return;

    initialized = 0;
    PRINT_DEBUG();

    FILE* f = fopen(DCMALLOC_OUTPUT_FILE, "w");
    if (!f)
    {
        PRINT_ERR("failed to open output file");
        return;
    }

    PRINT_DEBUG("Dumping histogram data...");
    for (uint32_t i = 0; i < MAX_ALLOC; ++i)
    {
        if (global_alloc_sizes[i] > 0)
            fprintf(f, "Alloc size %u: %zu mallocs\n", i, global_alloc_sizes[i]);
    }

#ifdef DCMALLOC_COLLECT_THREAD_BEHAVIOR
    PRINT_DEBUG("Dumping in-thread free histogram data...");
    for (uint32_t i = 0; i < MAX_ALLOC; ++i)
    {
        if (global_alloc_sizes_thread[i] > 0)
        {
            fprintf(f, "Alloc size: %u: %zu in-thread frees\n",
                    i, global_alloc_sizes_thread[i]);
        }
    }
#endif // DCMALLOC_COLLECT_THREAD_BEHAVIOR

    fclose(f);
}

void dc_malloc_thread_initialize()
{
    PRINT_DEBUG();
}

void dc_malloc_thread_finalize()
{
    PRINT_DEBUG();

    {
        pthread_mutex_lock(&global_alloc_mutex);
        for (uint32_t i = 0; i < MAX_ALLOC; ++i)
        {
            global_alloc_sizes[i] += alloc_sizes[i];
            global_alloc_sizes_thread[i] += alloc_sizes_thread[i];
        }

        pthread_mutex_unlock(&global_alloc_mutex);
    }
}

void update_alloc_counter(size_t size)
{
    if (initialized)
        ++alloc_sizes[min(size, MAX_ALLOC - 1)];
}

void update_alloc_thread_counter(alloc_data* data)
{
    if (initialized && pthread_equal(pthread_self(), data->tid))
        ++alloc_sizes_thread[min(data->size, MAX_ALLOC - 1)];
}

void update_ptr(void* ptr, alloc_data* data)
{
    ((alloc_data**)ptr)[-1] = data;
}

// retrieve alloc_data from free'd ptr
alloc_data* ptr2data(void* ptr)
{
    // | alloc_data | ... | ptr | alloc ... |
    // a ptr to alloc data is located on the word before alloc
    return ((alloc_data**)ptr)[-1];
}

void* dc_malloc(size_t size)
{
    static void* (*libc_malloc)(size_t) = NULL;
    if (libc_malloc == NULL)
        libc_malloc = dlsym(RTLD_NEXT, "malloc");

    size_t true_size = size;
    update_alloc_counter(true_size);

    PRINT_DEBUG("size: %lu", true_size);

#ifdef DCMALLOC_COLLECT_THREAD_BEHAVIOR
    // to keep data per allocation, need to increase allocation size
    size += sizeof(alloc_data);
#endif // DCMALLOC_COLLECT_THREAD_BEHAVIOR

    void* ptr = libc_malloc(size);
    if (ptr == NULL)
        return ptr;

#ifdef DCMALLOC_COLLECT_THREAD_BEHAVIOR
    alloc_data* data = (alloc_data*)ptr;
    data->tid = pthread_self();
    data->size = true_size;

    ptr = (void*)(ptr + sizeof(alloc_data));
    update_ptr(ptr, data);
#endif // DCMALLOC_COLLECT_THREAD_BEHAVIOR

    return ptr;
}

void dc_free(void* ptr)
{
    static void (*libc_free)(void*) = NULL;
    if (libc_free == NULL)
        libc_free = dlsym(RTLD_NEXT, "free");

    // ptr might have been given by static_malloc
    if (try_static_free(ptr))
        return;
    
    PRINT_DEBUG("ptr: %p", ptr);

#ifdef DCMALLOC_COLLECT_THREAD_BEHAVIOR
    if (ptr != NULL)
    {
        alloc_data* data = ptr2data(ptr);
        ptr = (void*)data;

        update_alloc_thread_counter(data);
    }
#endif // DCMALLOC_COLLECT_THREAD_BEHAVIOR

    return libc_free(ptr);
}

void* dc_calloc(size_t n, size_t size)
{
    static void* (*libc_calloc)(size_t, size_t) = NULL;
    if (libc_calloc == NULL)
    {
        // dlsym itself uses calloc, so we can enter a loop
        // to break it off, use a static memory allocator for
        //  dlsym allocations
        
        // @todo: gcc bug causes first 'true' write to be optimized out
        // need to mark variable as volatile
        static volatile bool fetching_calloc = false;
        if (!fetching_calloc)
        {
            fetching_calloc = true;
            assert(fetching_calloc == true);
            libc_calloc = dlsym(RTLD_NEXT, "calloc");
            fetching_calloc = false;
        }
        else
        {
            // dc_free also has handling for freeing this ptr
            return static_calloc(n, size);
        }
    }

    // @todo: same error checking as calloc
    size_t true_size = n * size;
    update_alloc_counter(true_size);

    PRINT_DEBUG("size: %lu", true_size);

#ifdef DCMALLOC_COLLECT_THREAD_BEHAVIOR
    // to keep data per allocation, need to increase allocation size
    n += size / sizeof(alloc_data) + 1;
#endif // DCMALLOC_COLLECT_THREAD_BEHAVIOR

    void* ptr = libc_calloc(n, size);
    if (ptr == NULL)
        return ptr;

#ifdef DCMALLOC_COLLECT_THREAD_BEHAVIOR
    alloc_data* data = (alloc_data*)ptr;
    data->tid = pthread_self();
    data->size = true_size;

    ptr = (void*)(ptr + sizeof(alloc_data));
    update_ptr(ptr, data);
#endif // DCMALLOC_COLLECT_THREAD_BEHAVIOR

    return ptr;
}

void* dc_realloc(void* ptr, size_t size)
{
    static void* (*libc_realloc)(void*, size_t) = NULL;
    if (libc_realloc == NULL)
        libc_realloc = dlsym(RTLD_NEXT, "realloc");

    size_t true_size = size;
    update_alloc_counter(true_size);

    PRINT_DEBUG("size: %lu", true_size);

#ifdef DCMALLOC_COLLECT_THREAD_BEHAVIOR
    // to keep data per allocation, need to increase allocation size
    size += sizeof(alloc_data);
    if (ptr)
        ptr = (void*)ptr2data(ptr);
#endif // DCMALLOC_COLLECT_THREAD_BEHAVIOR

    void* new_ptr = libc_realloc(ptr, size);
    if (new_ptr == NULL)
        return new_ptr;

#ifdef DCMALLOC_COLLECT_THREAD_BEHAVIOR
    alloc_data* data = (alloc_data*)new_ptr;
    data->tid = pthread_self();
    data->size = true_size;

    new_ptr = (void*)(new_ptr + sizeof(alloc_data));
    update_ptr(new_ptr, data);
#endif // DCMALLOC_COLLECT_THREAD_BEHAVIOR

    return new_ptr;
}

int dc_posix_memalign(void** memptr, size_t alignment, size_t size)
{
    static int (*libc_posix_memalign)(void**, size_t, size_t) = NULL;
    if (libc_posix_memalign == NULL)
        libc_posix_memalign = dlsym(RTLD_NEXT, "posix_memalign");

    size_t true_size = size;
    update_alloc_counter(true_size);

    PRINT_DEBUG("size: %lu", true_size);

#ifdef DCMALLOC_COLLECT_THREAD_BEHAVIOR
    // to keep data per allocation, need to increase allocation size
    size_t offset = (sizeof(alloc_data) / alignment + 1) * sizeof(alloc_data);
    size += offset;
#endif // DCMALLOC_COLLECT_THREAD_BEHAVIOR

    int ret = libc_posix_memalign(memptr, alignment, size);
    if (ret || *memptr == NULL)
        return ret;

#ifdef DCMALLOC_COLLECT_THREAD_BEHAVIOR
    void* ptr = *memptr;
    alloc_data* data = (alloc_data*)ptr;
    data->tid = pthread_self();
    data->size = true_size;

    ptr = (void*)(ptr + offset);
    update_ptr(ptr, data);
    *memptr = ptr;
#endif // DCMALLOC_COLLECT_THREAD_BEHAVIOR

    return ret;
}

void* dc_aligned_alloc(size_t alignment, size_t size)
{
    static void* (*libc_aligned_alloc)(size_t, size_t) = NULL;
    if (libc_aligned_alloc == NULL)
        libc_aligned_alloc = dlsym(RTLD_NEXT, "aligned_alloc");

    size_t true_size = size;
    update_alloc_counter(true_size);

    PRINT_DEBUG("size: %lu", true_size);

#ifdef DCMALLOC_COLLECT_THREAD_BEHAVIOR
    // to keep data per allocation, need to increase allocation size
    size_t offset = (sizeof(alloc_data) / alignment + 1) * sizeof(alloc_data);
    size += offset;
#endif // DCMALLOC_COLLECT_THREAD_BEHAVIOR

    void* ptr = libc_aligned_alloc(alignment, size);
    if (ptr == NULL)
        return ptr;

#ifdef DCMALLOC_COLLECT_THREAD_BEHAVIOR
    alloc_data* data = (alloc_data*)ptr;
    data->tid = pthread_self();
    data->size = true_size;

    ptr = (void*)(ptr + offset);
    update_ptr(ptr, data);
#endif // DCMALLOC_COLLECT_THREAD_BEHAVIOR

    return ptr;
}

void* dc_valloc(size_t size)
{
    static void* (*libc_valloc)(size_t) = NULL;
    if (libc_valloc == NULL)
        libc_valloc = dlsym(RTLD_NEXT, "aligned_alloc");

    size_t true_size = size;
    update_alloc_counter(true_size);

    PRINT_DEBUG("size: %lu", true_size);

#ifdef DCMALLOC_COLLECT_THREAD_BEHAVIOR
    // to keep data per allocation, need to increase allocation size
    size_t offset = (sizeof(alloc_data) / PAGE + 1) * sizeof(alloc_data);
    size += offset;
#endif // DCMALLOC_COLLECT_THREAD_BEHAVIOR

    void* ptr = libc_valloc(size);
    if (ptr == NULL)
        return ptr;

#ifdef DCMALLOC_COLLECT_THREAD_BEHAVIOR
    alloc_data* data = (alloc_data*)ptr;
    data->tid = pthread_self();
    data->size = true_size;

    ptr = (void*)(ptr + offset);
    update_ptr(ptr, data);
#endif // DCMALLOC_COLLECT_THREAD_BEHAVIOR

    return ptr;
}

void* dc_memalign(size_t alignment, size_t size)
{
    static void* (*libc_memalign)(size_t, size_t) = NULL;
    if (libc_memalign == NULL)
        libc_memalign = dlsym(RTLD_NEXT, "memalign");

    size_t true_size = size;
    update_alloc_counter(true_size);

    PRINT_DEBUG("size: %lu", true_size);

#ifdef DCMALLOC_COLLECT_THREAD_BEHAVIOR
    // to keep data per allocation, need to increase allocation size
    size_t offset = (sizeof(alloc_data) / alignment + 1) * sizeof(alloc_data);
    size += offset;
#endif // DCMALLOC_COLLECT_THREAD_BEHAVIOR

    void* ptr = libc_memalign(alignment, size);
    if (ptr == NULL)
        return ptr;

#ifdef DCMALLOC_COLLECT_THREAD_BEHAVIOR
    alloc_data* data = (alloc_data*)ptr;
    data->tid = pthread_self();
    data->size = true_size;

    ptr = (void*)(ptr + offset);
    update_ptr(ptr, data);
#endif // DCMALLOC_COLLECT_THREAD_BEHAVIOR

    return ptr;
}

void* dc_pvalloc(size_t size)
{
    static void* (*libc_pvalloc)(size_t) = NULL;
    if (libc_pvalloc == NULL)
        libc_pvalloc = dlsym(RTLD_NEXT, "pvalloc");

    size_t true_size = size;
    update_alloc_counter(true_size);

    PRINT_DEBUG("size: %lu", true_size);

#ifdef DCMALLOC_COLLECT_THREAD_BEHAVIOR
    // to keep data per allocation, need to increase allocation size
    size_t offset = (sizeof(alloc_data) / PAGE + 1) * sizeof(alloc_data);
    size += offset;
#endif // DCMALLOC_COLLECT_THREAD_BEHAVIOR

    void* ptr = libc_pvalloc(size);
    if (ptr == NULL)
        return ptr;

#ifdef DCMALLOC_COLLECT_THREAD_BEHAVIOR
    alloc_data* data = (alloc_data*)ptr;
    data->tid = pthread_self();
    data->size = true_size;

    ptr = (void*)(ptr + offset);
    update_ptr(ptr, data);
#endif // DCMALLOC_COLLECT_THREAD_BEHAVIOR

    return ptr;
}

/*
 * End dcmalloc functions.
 */

/*
 * Begin externals hooks to dcmalloc
 */

// handle process init/exit hooks
static pthread_key_t destructor_key;

static void* thread_initializer(void*);
static void thread_finalizer(void*);

static DCMALLOC_ATTR(constructor)
void initializer()
{
    static int is_initialized = 0;
    if (is_initialized == 0)
    {
        is_initialized = 1;
        pthread_key_create(&destructor_key, thread_finalizer);
    }

    dc_malloc_initialize();
    dc_malloc_thread_initialize();
}

static DCMALLOC_ATTR(destructor)
void finalizer()
{
    dc_malloc_thread_finalize();
    dc_malloc_finalize();
}

// handle thread init/exit hooks
typedef struct
{
    void* (*real_start)(void*);
    void* real_arg;
} thread_starter_arg;

static void* thread_initializer(void* argptr)
{
    thread_starter_arg* arg = (thread_starter_arg*)argptr;
    void* (*real_start)(void*) = arg->real_start;
    void* real_arg = arg->real_arg;
    dc_malloc_thread_initialize();

    pthread_setspecific(destructor_key, (void*)1);
    return (*real_start)(real_arg);
}

static void thread_finalizer(void* value)
{
    dc_malloc_thread_finalize();
}

int pthread_create(pthread_t* thread,
                   pthread_attr_t const* attr,
                   void* (start_routine)(void*),
                   void* arg)
{
    static int (*pthread_create_fn)(pthread_t*,
                                    pthread_attr_t const*,
                                    void* (void*),
                                    void*) = NULL;
    if (pthread_create_fn == NULL)
        pthread_create_fn = dlsym(RTLD_NEXT, "pthread_create");

    // @todo: don't want to use malloc here
    // instead using a ringbuffer, which has limited storage
#define RING_BUFFER_SIZE 10000
    static uint32_t _Atomic ring_buffer_pos = 0;
    static thread_starter_arg ring_buffer[RING_BUFFER_SIZE];
    uint32_t buffer_pos = atomic_fetch_add_explicit(&ring_buffer_pos, 1, memory_order_relaxed);

    thread_starter_arg* starter_arg = &ring_buffer[buffer_pos];
    starter_arg->real_start = start_routine;
    starter_arg->real_arg = arg;
    dc_malloc_thread_initialize();
    return pthread_create_fn(thread, attr, thread_initializer, starter_arg);
}

