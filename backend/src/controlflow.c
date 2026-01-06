#include <stdlib.h>
#include <stdio.h>
#include "ir.h"
#include "layout.h"
#include "bpf_builder.h"
#include "ccbpf.h"
#include "controlflow.h"


static u_short relop_to_bpf(enum IR_RelOp r)
{
    switch (r) {
    case IR_GT: return BPF_JMP | BPF_JGT | BPF_X; /* A > X -> true */
    case IR_GE: return BPF_JMP | BPF_JGE | BPF_X; /* A >= X -> true */
    case IR_EQ: return BPF_JMP | BPF_JEQ | BPF_X; /* A == X -> true */
    case IR_NE: return BPF_JMP | BPF_JEQ | BPF_X; /* A != X -> false if EQ */
    }
    abort();
}

/*
 * IR_IF_FALSE: if (!(a relop b)) goto label;
 */
void lower_if_false(const struct backend_layout *l,
                    struct bpf_builder *b,
                    struct IR *ir,
                    struct pending **pj,
                    int *pj_count, int *pj_cap)
{
    int a     = temp_slot(l, ir->src1);
    int bslot = temp_slot(l, ir->src2);
    u_short jop;
    int insn;

    bpf_builder_emit(b,
        (struct bpf_insn)BPF_STMT(BPF_LD  | BPF_MEM, a));
    bpf_builder_emit(b,
        (struct bpf_insn)BPF_STMT(BPF_LDX | BPF_MEM, bslot));

    jop = relop_to_bpf(ir->relop);

    insn = bpf_builder_emit(b,
        (struct bpf_insn)BPF_JUMP(jop, 0, 0, 0));

    if (*pj_count >= *pj_cap) {
        *pj_cap = *pj_cap ? (*pj_cap * 2) : 16;
        *pj = realloc(*pj, *pj_cap * sizeof(**pj));
        if (!*pj)
            abort();
    }

    int true_branch = 0;         
    if (ir->relop == IR_NE)
        true_branch = 1;       

    (*pj)[*pj_count].insn        = insn;
    (*pj)[*pj_count].label       = ir->label;
    (*pj)[*pj_count].is_cond     = 1;
    (*pj)[*pj_count].true_branch = true_branch;
    (*pj_count)++;
}

void lower_goto(struct bpf_builder *b,
                struct IR *ir,
                struct pending **pj,
                int *pj_count, int *pj_cap)
{
    int insn = bpf_builder_emit(b,
        (struct bpf_insn)BPF_JUMP(BPF_JMP | BPF_JA, 0, 0, 0));

    if (*pj_count >= *pj_cap) {
        *pj_cap = *pj_cap ? (*pj_cap * 2) : 16;
        *pj = realloc(*pj, *pj_cap * sizeof(**pj));
        if (!*pj)
            abort();
    }

    (*pj)[*pj_count].insn        = insn;
    (*pj)[*pj_count].label       = ir->label;
    (*pj)[*pj_count].is_cond     = 0;
    (*pj)[*pj_count].true_branch = 0;
    (*pj_count)++;
}

void lower_label(int *label_pc,
                 struct bpf_builder *b,
                 struct IR *ir)
{
    label_pc[ir->label] = bpf_builder_count(b);
}


void patch_jumps(struct bpf_builder *b,
                        struct pending *pj, int pj_count,
                        int *label_pc)
{
    struct bpf_insn *ins = bpf_builder_data(b);
    int n = bpf_builder_count(b);   
    int i;

    printf("patch_jumps: pj_count=%d, insn_count=%d\n", pj_count, n);

    for (i = 0; i < pj_count; i++) {
        printf(" pending[%d]: insn=%d, label=%d, is_cond=%d, true_branch=%d\n",
               i, pj[i].insn, pj[i].label, pj[i].is_cond, pj[i].true_branch);
    }

    for (i = 0; i < pj_count; i++) {
        int from  = pj[i].insn;
        int label = pj[i].label;

        if (from < 0 || from >= n) {
            fprintf(stderr, "bad pending insn: insn=%d, insn_count=%d\n",
                    from, n);
            abort();
        }

        int to  = label_pc[label];
        int rel = to - (from + 1);

        printf("  patch[%d]: from=%d -> to=%d (rel=%d)\n",
               i, from, to, rel);

        if (pj[i].is_cond) {
            if (pj[i].true_branch)
                ins[from].jt = (u_char)rel;
            else
                ins[from].jf = (u_char)rel;
        } else {
            ins[from].k = rel;
        }
    }
}

