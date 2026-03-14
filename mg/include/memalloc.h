#ifndef _MEMALLOC_H_
#define _MEMALLOC_H_

#include <stddef.h>
#include <stdint.h>

void *mem_malloc(size_t size);
void mem_free(void *ptr);

typedef uint64_t ptr_size_t;

#endif
