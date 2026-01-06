#ifndef SELECTION_H
#define SELECTION_H

#include "layout.h"
#include "ir.h"
#include "bpf_builder.h"

void lower_move(const struct backend_layout *l,
                struct bpf_builder *b, struct IR *ir);

void lower_binop(const struct backend_layout *l,
                 struct bpf_builder *b, struct IR *ir);

void lower_load(const struct backend_layout *l,
                struct bpf_builder *b, struct IR *ir);

void lower_store(const struct backend_layout *l,
                 struct bpf_builder *b, struct IR *ir);

#endif
