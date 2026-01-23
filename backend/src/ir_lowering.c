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

        case IR_RET: {
            int slot = temp_slot(&layout, ir->src1);

            bpf_builder_emit(b,
                (struct bpf_insn)BPF_STMT(BPF_LD | BPF_MEM, slot));

            bpf_builder_emit(b,
                (struct bpf_insn)BPF_STMT(BPF_RET | BPF_A, 0));
            break;
        }

        case IR_LOAD_CTX: {
            int offset = ir->src1;
            int size   = ir->src2;
            int dst    = temp_slot(&layout, ir->dst);

            switch (size) {
            case 1:
                bpf_builder_emit(b,
                    (struct bpf_insn)BPF_STMT(BPF_LD | BPF_B | BPF_ABS, offset));
                break;
            case 2:
                bpf_builder_emit(b,
                    (struct bpf_insn)BPF_STMT(BPF_LD | BPF_H | BPF_ABS, offset));
                break;
            case 4:
                bpf_builder_emit(b,
                    (struct bpf_insn)BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offset));
                break;
            default:
                fprintf(stderr, "invalid IR_LOAD_CTX size\n");
                exit(1);
            }

            bpf_builder_emit(b,
                (struct bpf_insn)BPF_STMT(BPF_ST, dst));
            break;
        }

        case IR_NATIVE_CALL: {
            int dst_slot  = temp_slot(&layout, ir->dst);
            int arg0_slot = temp_slot(&layout, ir->args[0]);

            /* A = MEM[arg0_slot] */
            bpf_builder_emit(b,
                (struct bpf_insn)BPF_STMT(BPF_LD | BPF_MEM, arg0_slot));

            /* Host-Call: code = BPF_MISC | BPF_COP, k = func_id */
            bpf_builder_emit(b,
                (struct bpf_insn)BPF_STMT(BPF_MISC | BPF_COP,
                                          ir->func_id));

            /* A → MEM[dst_slot] */
            bpf_builder_emit(b,
                (struct bpf_insn)BPF_STMT(BPF_ST, dst_slot));
            break;
        }

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
