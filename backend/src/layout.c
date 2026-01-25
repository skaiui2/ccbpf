#include "layout.h"
#include "ir_lowering.h"
#include <stdlib.h>

struct backend_layout default_bpf_layout(void)
{
    struct backend_layout l = {
        .temp_base  = 64,
        .temp_count = 64,
        .mem_a = 8,   // x
        .mem_b = 16,  // y
        .mem_c = 24,  // key or uh 

        .mem_arr_base = {
            32, // MEM_ARR0
            40, // MEM_ARR1
            48, // MEM_ARR2
            56, // MEM_ARR3
        },
    };
    return l;
}

int temp_slot(const struct backend_layout *l, int t)
{
    return l->temp_base + t * sizeof(u_long);
}

int map_array_base(const struct backend_layout *l, int base)
{
    switch (base) {
    case 0:  return l->mem_a;
    case 4:  return l->mem_b;
    case 8:  return l->mem_c;
    case 12: return l->mem_arr_base[0];
    case 16: return l->mem_arr_base[1];
    case 20: return l->mem_arr_base[2];
    case 24: return l->mem_arr_base[3];
    default:
        abort();
    }
}
