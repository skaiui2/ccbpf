#include <stdlib.h>
#include <stdio.h>
#include "ir.h"
#include "layout.h"
#include "bpf_builder.h"
#include "cbpf.h"
#include "controlflow.h"

static int pending_count_global = 0;
static int pending_peak_global  = 0;
static size_t pending_bytes_global = 0;

static void pending_add(struct pending *pj, int *pj_count,
                        int insn, int label, int is_cond, int true_branch)
{
    if (*pj_count >= MAX_PENDING_JUMPS) {
        fprintf(stderr, "[FATAL] pending overflow (%d/%d)\n",
                *pj_count, MAX_PENDING_JUMPS);
        abort();
    }

    struct pending *p = &pj[*pj_count];

    p->insn        = insn;
    p->label       = label;
    p->is_cond     = is_cond;
    p->true_branch = true_branch;

    (*pj_count)++;

    pending_count_global++;
    pending_bytes_global += sizeof(struct pending);
    if (pending_count_global > pending_peak_global)
        pending_peak_global = pending_count_global;
}

static unsigned short relop_to_bpf(enum IR_RelOp r)
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
                    int *pj_count)
{
    int a     = temp_slot(l, ir->src1);
    int bslot = temp_slot(l, ir->src2);
    unsigned short jop;
    int insn;

    bpf_builder_emit(b,
        (struct bpf_insn)BPF_STMT(BPF_LD  | BPF_MEM, a));
    bpf_builder_emit(b,
        (struct bpf_insn)BPF_STMT(BPF_LDX | BPF_MEM, bslot));

    jop = relop_to_bpf(ir->relop);

    insn = bpf_builder_emit(b,
        (struct bpf_insn)BPF_JUMP(jop, 0, 0, 0));

    int true_branch = 0;         
    if (ir->relop == IR_NE)
        true_branch = 1;       

    pending_add(*pj, pj_count, insn, ir->label, 1, true_branch);
}

void lower_goto(struct bpf_builder *b,
                struct IR *ir,
                struct pending **pj,
                int *pj_count)
{
    int insn = bpf_builder_emit(b,
        (struct bpf_insn)BPF_JUMP(BPF_JMP | BPF_JA, 0, 0, 0));

    pending_add(*pj, pj_count, insn, ir->label, 0, 0);
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
                ins[from].jt = (unsigned char)rel;
            else
                ins[from].jf = (unsigned char)rel;
        } else {
            ins[from].k = rel;
        }
    }
    printf("[MEM] pending_jumps: count=%d, peak=%d, bytes=%zu (cap=%d entries)\n",
       pending_count_global,
       pending_peak_global,
       pending_bytes_global,
       MAX_PENDING_JUMPS);
}

