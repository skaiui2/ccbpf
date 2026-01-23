#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "ccbpf.h" 

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


uint32_t ccbpf_run_frame(struct ccbpf_program *p,
                         void *frame,
                         size_t frame_size)
{
    return bpf_filter(p->insns,
                      (unsigned char *)frame,
                      frame_size,
                      frame_size);
}
