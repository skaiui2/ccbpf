#ifndef CCBPF_H
#define CCBPF_H

#include <stdint.h>
#include "hashmap.h"
#include "cbpf.h"

uint32_t ccbpf_run_frame(struct ccbpf_program *p,
                         void *frame,
                         size_t frame_size);

struct ccbpf_program *ccbpf_load(const char *path);
void ccbpf_unload(struct ccbpf_program *p);
uint32_t ccbpf_run(struct ccbpf_program *p);

#endif
