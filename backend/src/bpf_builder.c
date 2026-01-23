#include "bpf_builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

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


void write_ccbpf(const char *path, struct bpf_insn *insns, size_t insn_count)
{
    struct CCBPF_Header hdr = {0};

    hdr.magic       = CCBPF_MAGIC;
    hdr.version     = 1;
    hdr.flags       = 0;

    hdr.code_offset = sizeof(struct CCBPF_Header);
    hdr.code_size   = (uint32_t)(insn_count * sizeof(struct bpf_insn));

    hdr.data_offset = 0;
    hdr.data_size   = 0;

    hdr.entry       = 0; 

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        perror("fopen ccbpf");
        return;
    }

    fwrite(&hdr, sizeof(hdr), 1, fp);
    fwrite(insns, sizeof(struct bpf_insn), insn_count, fp);

    fclose(fp);
}

