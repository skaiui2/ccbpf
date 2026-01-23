#ifndef CCBPF_H
#define CCBPF_H

#include <stdint.h>
#include "cbpf.h"

#define CCBPF_MAGIC 0x43434250  /* 'C' 'C' 'B' 'P' */

struct CCBPF_Header {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;

    uint32_t code_offset;
    uint32_t code_size;

    uint32_t data_offset;
    uint32_t data_size;

    uint32_t entry;
};

struct ccbpf_program {
    struct bpf_insn *insns; //.text
    size_t insn_count;

    uint8_t *data;     //.data or .radata
    size_t data_size;

    uint32_t entry;
};

struct hook_ctx { 
    uint32_t arg0; 
    uint32_t arg1; 
}; 

uint32_t ccbpf_run_ctx(struct ccbpf_program *p, void *ctx, size_t ctx_size);
uint32_t ccbpf_run_pkt(struct ccbpf_program *p, uint8_t *pkt, size_t len);

struct ccbpf_program ccbpf_load(const char *path);
void ccbpf_unload(struct ccbpf_program *p);
uint32_t ccbpf_run(struct ccbpf_program *p);

#endif
