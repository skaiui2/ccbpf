#include "bpf_builder.h"
#include <stdlib.h>
#include <string.h>

void bpf_builder_init(struct bpf_builder *b)
{
    b->count = 0;
    b->capacity = 128;

    b->insns = malloc(sizeof(struct bpf_insn) * b->capacity);
    if (!b->insns)
        abort();
}

void bpf_builder_free(struct bpf_builder *b)
{
    if (b->insns)
        free(b->insns);

    b->insns = NULL;
    b->count = 0;
    b->capacity = 0;
}

void bpf_builder_reset(struct bpf_builder *b)
{
    b->count = 0;
}

static void bpf_builder_grow(struct bpf_builder *b)
{
    int new_cap = b->capacity * 2;
    struct bpf_insn *new_insns;

    new_insns = realloc(b->insns,
                sizeof(struct bpf_insn) * new_cap);
    if (!new_insns)
        abort();

    b->insns = new_insns;
    b->capacity = new_cap;
}

int bpf_builder_emit(struct bpf_builder *b, struct bpf_insn insn)
{
    if (b->count >= b->capacity)
        bpf_builder_grow(b);

    b->insns[b->count] = insn;
    return b->count++;
}

struct bpf_insn *bpf_builder_data(struct bpf_builder *b)
{
    return b->insns;
}

int bpf_builder_count(struct bpf_builder *b)
{
    return b->count;
}
