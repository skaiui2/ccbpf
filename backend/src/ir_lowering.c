#include "ir_lowering.h"
#include "layout.h"
#include "selection.h"
#include "controlflow.h"
#include <stdlib.h>
#include <stdio.h>

void ir_lower_program(struct IR *head, int label_count,
                      struct bpf_builder *b)
{
    struct backend_layout layout = default_bpf_layout();

    struct pending *pj = NULL;
    int pj_count = 0, pj_cap = 0;

    int *label_pc = calloc(label_count, sizeof(int));

    for (struct IR *ir = head; ir; ir = ir->next) {
        switch (ir->op) {

        case IR_MOVE:
            lower_move(&layout, b, ir);
            break;

        case IR_ADD:
        case IR_SUB:
        case IR_MUL:
            lower_binop(&layout, b, ir);
            break;

        case IR_LOAD:
            lower_load(&layout, b, ir);
            break;

        case IR_STORE:
            lower_store(&layout, b, ir);
            break;

        case IR_IF_FALSE:
            lower_if_false(&layout, b, ir,
                           &pj, &pj_count, &pj_cap);
            break;

        case IR_GOTO:
            lower_goto(b, ir,
                       &pj, &pj_count, &pj_cap);
            break;

        case IR_LABEL:
            lower_label(label_pc, b, ir);
            break;

        default:
            abort();
        }
    }

    patch_jumps(b, pj, pj_count, label_pc);

	int n = bpf_builder_count(b);
	struct bpf_insn *prog = bpf_builder_data(b);
	printf("BPF program (%d insns):\n", n);
	for (int i = 0; i < n; i++) {
	    printf("%3d: code=0x%04x jt=%u jf=%u k=%ld\n",
           i, prog[i].code, prog[i].jt, prog[i].jf, prog[i].k);
	}

    free(label_pc);
    free(pj);
}
