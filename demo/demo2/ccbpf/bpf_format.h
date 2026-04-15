#ifndef _BPF_FORMAT_H
#define _BPF_FORMAT_H

#include <stdint.h>

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

#endif

