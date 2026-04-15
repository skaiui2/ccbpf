#ifndef _MEMBIT_H
#define _MEMBIT_H

#include <stdint.h>
#include <stddef.h>

struct membit_pool {
    uint32_t bitmask;
    size_t block_size;
    uint8_t count;
};

typedef struct membit_pool *membit_pool_t;

membit_pool_t membit_create(uint16_t size, uint8_t count);
void *membit_alloc(membit_pool_t pool);
void membit_free(membit_pool_t pool, void *addr);
void membit_destroy(membit_pool_t pool);
void membit_reset(membit_pool_t pool);

#endif
