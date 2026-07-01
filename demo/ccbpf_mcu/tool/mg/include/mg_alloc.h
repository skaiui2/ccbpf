#ifndef _MG_ALLOC_H
#define _MG_ALLOC_H

#include <stddef.h>
#include <stdint.h>

struct mg_region;
typedef struct mg_region *mg_region_handle;

mg_region_handle mg_region_create_pool(uint8_t max_class_bits);
mg_region_handle mg_region_create_bump(size_t cap);
void            *mg_region_alloc(mg_region_handle r, size_t size);
void             mg_region_reset(mg_region_handle r);
void             mg_region_destroy(mg_region_handle r);
void             mg_region_print_pools(mg_region_handle r);



#endif
