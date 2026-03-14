#ifndef _HEAP_H_
#define _HEAP_H_

#include <stddef.h>
#include <stdint.h>

#define HEAP_TRACKING 0

struct heap_stats {
    size_t remain_size;
    size_t free_size_iter;
    size_t max_free_block;
    size_t free_blocks;
};

void   heap_init(void);
void  *heap_malloc_dbg(size_t size, const char *file, int line);
void   heap_free_dbg(void *ptr, const char *file, int line);
struct heap_stats heap_get_stats(void);
void   heap_debug_dump_leaks(void);

#if HEAP_TRACKING
#define heap_malloc(size) heap_malloc_dbg(size, __FILE__, __LINE__)
#define heap_free(ptr)    heap_free_dbg(ptr, __FILE__, __LINE__)
#else
#define heap_malloc(size) heap_malloc_dbg(size, NULL, 0)
#define heap_free(ptr)    heap_free_dbg(ptr, NULL, 0)
#endif

#endif
