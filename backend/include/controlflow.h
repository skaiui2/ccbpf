#ifndef CONTROLFLOW_H
#define CONTROLFLOW_H

#include "layout.h"
#include "ir.h"
#include "bpf_builder.h"

struct pending {
    int insn;
    int label;
    int is_cond;
    int true_branch;
};

void lower_if_false(const struct backend_layout *l,
                    struct bpf_builder *b,
                    struct IR *ir,
                    struct pending **pj,
                    int *pj_count, int *pj_cap);

void lower_goto(struct bpf_builder *b,
                struct IR *ir,
                struct pending **pj,
                int *pj_count, int *pj_cap);

void lower_label(int *label_pc,
                 struct bpf_builder *b,
                 struct IR *ir);

void patch_jumps(struct bpf_builder *b,
                 struct pending *pj, int pj_count,
                 int *label_pc);

#endif
