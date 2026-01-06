#include "layout.h"
#include "ir_lowering.h"
#include <stdlib.h>

struct backend_layout default_bpf_layout(void)
{
    struct backend_layout l = {
        .temp_base  = 8,
        .temp_count = 64,

        .mem_a = MEM_A,
        .mem_b = MEM_B,
        .mem_c = MEM_C,

        .mem_arr_base = {
            MEM_ARR0,
            MEM_ARR1,
            MEM_ARR2,
            MEM_ARR3,
        },
    };
    return l;
}

int temp_slot(const struct backend_layout *l, int t)
{
    return l->temp_base + t;
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
