#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "ccbpf.h"
#include "bpf_builder.h"  

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


struct ccbpf_program ccbpf_load(const char *path)
{
    struct ccbpf_program prog = {0};

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("ccbpf_load fopen");
        return prog;
    }

    struct CCBPF_Header hdr;
    fread(&hdr, sizeof(hdr), 1, fp);

    if (hdr.magic != CCBPF_MAGIC) {
        fprintf(stderr, "Invalid CCBPF magic\n");
        fclose(fp);
        return prog;
    }

    fseek(fp, hdr.code_offset, SEEK_SET);
    size_t insn_count = hdr.code_size / sizeof(struct bpf_insn);

    struct bpf_insn *insns = malloc(hdr.code_size);
    fread(insns, sizeof(struct bpf_insn), insn_count, fp);

    uint8_t *data = NULL;
    if (hdr.data_size > 0) {
        fseek(fp, hdr.data_offset, SEEK_SET);
        data = malloc(hdr.data_size);
        fread(data, 1, hdr.data_size, fp);
    }

    fclose(fp);

    prog.insns = insns;
    prog.insn_count = insn_count;
    prog.data = data;
    prog.data_size = hdr.data_size;
    prog.entry = hdr.entry;

    return prog;
}

void ccbpf_unload(struct ccbpf_program *p)
{
    if (!p)
        return;

    if (p->insns) {
        free(p->insns);
        p->insns = NULL;
    }

    if (p->data) {
        free(p->data);
        p->data = NULL;
    }

    p->insn_count = 0;
    p->data_size = 0;
    p->entry = 0;
}


uint32_t ccbpf_run(struct ccbpf_program *p)
{
    unsigned char dummy[1] = {0};
    return bpf_filter(p->insns, dummy, 0, 0);
}

uint32_t ccbpf_run_ctx(struct ccbpf_program *p, void *ctx, size_t ctx_size)
{
    uint32_t tmp[2];
    struct hook_ctx *c = (struct hook_ctx *)ctx;

    tmp[0] = htonl(c->arg0);
    tmp[1] = htonl(c->arg1);

    return bpf_filter(p->insns, (unsigned char *)tmp, sizeof(tmp), sizeof(tmp));
}

uint32_t ccbpf_run_pkt(struct ccbpf_program *p, uint8_t *pkt, size_t len)
{
    return bpf_filter(p->insns, pkt, len, len);
}
