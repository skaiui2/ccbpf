#include "membit.h"
#include "heap.h"

#define ALIGN_BYTE 0x07

static const size_t head_size =
        (sizeof(struct membit_pool) + ALIGN_BYTE) & ~ALIGN_BYTE;

static inline uint8_t lowbit_index(uint32_t v)
{
    return __builtin_ctz(v);
}

membit_pool_t membit_create(uint16_t size, uint8_t count)
{
    struct membit_pool *p;
    size_t total;

    if (size & ALIGN_BYTE) {
        size += ALIGN_BYTE;
        size &= ~ALIGN_BYTE;
    }

    if (count > 32)
        count = 32;

    total = head_size + (size_t)size * count;
    p = heap_malloc(total);
    if (!p)
        return NULL;

    p->block_size = size;
    p->count = count;
    p->bitmask = (count == 32) ? 0xFFFFFFFFu : ((1u << count) - 1u);

    return p;
}

void *membit_alloc(membit_pool_t pool)
{
    uint8_t idx;
    uint8_t *base;

    if (!pool || !pool->bitmask)
        return NULL;

    idx = lowbit_index(pool->bitmask);
    pool->bitmask &= ~(1u << idx);

    base = (uint8_t *)pool + head_size;
    return base + pool->block_size * idx;
}

void membit_free(membit_pool_t pool, void *addr)
{
    size_t off;
    uint8_t idx;

    if (!pool || !addr)
        return;

    off = (size_t)addr - ((size_t)pool + head_size);
    idx = off / pool->block_size;

    pool->bitmask |= (1u << idx);
}

void membit_destroy(membit_pool_t pool)
{
    if (pool)
        heap_free(pool);
}

void membit_reset(membit_pool_t pool)
{
    if (!pool)
        return;

    if (pool->count == 32)
        pool->bitmask = 0xFFFFFFFFu;
    else
        pool->bitmask = (1u << pool->count) - 1u;
}
