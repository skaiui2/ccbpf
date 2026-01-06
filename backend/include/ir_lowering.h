#ifndef IR_LOWERING_H
#define IR_LOWERING_H

#include "ccbpf.h"
#include "bpf_builder.h"
#include "ir.h"


#define MEM_A      0
#define MEM_B      1
#define MEM_C      2
#define MEM_ARR0   3
#define MEM_ARR1   4
#define MEM_ARR2   5
#define MEM_ARR3   6

void ir_lower_program(struct IR *head, int label_count, struct bpf_builder *b);


#endif