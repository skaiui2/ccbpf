#ifndef LAYOUT_H
#define LAYOUT_H

struct backend_layout {
    int temp_base;
    int temp_count;

    int mem_a;
    int mem_b;
    int mem_c;

    int mem_arr_base[4];
};

struct backend_layout default_bpf_layout(void);

int temp_slot(const struct backend_layout *l, int t);
int map_array_base(const struct backend_layout *l, int base);

#endif
