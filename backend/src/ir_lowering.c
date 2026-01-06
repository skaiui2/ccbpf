#include <stdlib.h>
#include "ir_lowering.h"
#include <stdio.h>

/*
 * temp_no -> mem 
 */
static inline int temp_slot(int t)
{
	return 8 + t;   /* t1->9, t2->10, ... */
}

/*
 * IR 的 array_base是offset
 * 把它映射到mem槽位
 */
static inline int map_array_base(int base)
{
	switch (base) {
	case 0:  return MEM_A;
	case 4:  return MEM_B;
	case 8:  return MEM_C;
	case 12: return MEM_ARR0;
	case 16: return MEM_ARR1;
	case 20: return MEM_ARR2;
	case 24: return MEM_ARR3;
	default:
		abort();
	}
}

/*
 * relop -> BPF opcode
 */
static u_short relop_to_bpf(enum IR_RelOp r)
{
	switch (r) {
	case IR_LT: return BPF_JMP | BPF_JGE | BPF_X; /* !(A < X) -> A>=X */
	case IR_LE: return BPF_JMP | BPF_JGT | BPF_X; /* !(A <= X) -> A>X */
	case IR_GT: return BPF_JMP | BPF_JGT | BPF_X; /* A > X */
	case IR_GE: return BPF_JMP | BPF_JGE | BPF_X; /* A >= X */
	case IR_EQ: return BPF_JMP | BPF_JEQ | BPF_X;
	case IR_NE: return BPF_JMP | BPF_JEQ | BPF_X;
	}
	abort();
}

/*
 * pending jumps
 */
struct pending {
	int insn;
	int label;
	int is_cond;
	int true_branch;
};

static void patch_jumps(struct bpf_builder *b,
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

void ir_lower_program(struct IR *head, int label_count,
		      struct bpf_builder *b)
{
	struct pending *pj = NULL;
	int pj_count = 0, pj_cap = 0;

	int *label_pc = calloc(label_count, sizeof(int));
	if (!label_pc)
		abort();

	for (struct IR *ir = head; ir; ir = ir->next) {

		switch (ir->op) {

		case IR_MOVE: {
			int dst = temp_slot(ir->dst);

			bpf_builder_emit(b,
				(struct bpf_insn)BPF_STMT(BPF_LD | BPF_IMM,
							  ir->src1));
			bpf_builder_emit(b,
				(struct bpf_insn)BPF_STMT(BPF_ST, dst));
			break;
		}

		case IR_ADD:
		case IR_SUB:
		case IR_MUL: {
			int dst   = temp_slot(ir->dst);
			int a     = temp_slot(ir->src1);
			int bslot = temp_slot(ir->src2);

			u_short op =
				(ir->op == IR_ADD) ? (BPF_ALU | BPF_ADD | BPF_X) :
				(ir->op == IR_SUB) ? (BPF_ALU | BPF_SUB | BPF_X) :
						     (BPF_ALU | BPF_MUL | BPF_X);

			bpf_builder_emit(b,
				(struct bpf_insn)BPF_STMT(BPF_LD  | BPF_MEM, a));
			bpf_builder_emit(b,
				(struct bpf_insn)BPF_STMT(BPF_LDX | BPF_MEM,
							  bslot));
			bpf_builder_emit(b,
				(struct bpf_insn)BPF_STMT(op, 0));
			bpf_builder_emit(b,
				(struct bpf_insn)BPF_STMT(BPF_ST, dst));
			break;
		}

		case IR_LOAD: {
			int dst  = temp_slot(ir->dst);
			int base = map_array_base(ir->array_base);
			int slot = base + ir->array_index;

			bpf_builder_emit(b,
				(struct bpf_insn)BPF_STMT(BPF_LD | BPF_MEM,
							  slot));
			bpf_builder_emit(b,
				(struct bpf_insn)BPF_STMT(BPF_ST, dst));
			break;
		}

		case IR_STORE: {
			int src  = temp_slot(ir->src1);
			int base = map_array_base(ir->array_base);
			int slot = base + ir->array_index;

			bpf_builder_emit(b,
				(struct bpf_insn)BPF_STMT(BPF_LD | BPF_MEM,
							  src));
			bpf_builder_emit(b,
				(struct bpf_insn)BPF_STMT(BPF_ST, slot));
			break;
		}

		case IR_RELOP:
			break;

		case IR_IF_FALSE: {
			int a     = temp_slot(ir->src1);
			int bslot = temp_slot(ir->src2);
			u_short jop;
			int insn;

			bpf_builder_emit(b,
				(struct bpf_insn)BPF_STMT(BPF_LD  | BPF_MEM,
							  a));
			bpf_builder_emit(b,
				(struct bpf_insn)BPF_STMT(BPF_LDX | BPF_MEM,
							  bslot));

			jop = relop_to_bpf(ir->relop);

			insn = bpf_builder_emit(b,
				(struct bpf_insn)BPF_JUMP(jop, 0, 0, 0));

			if (pj_count >= pj_cap) {
				pj_cap = pj_cap ? pj_cap * 2 : 16;
				pj = realloc(pj,
					     pj_cap * sizeof(*pj));
				if (!pj)
					abort();
			}

			pj[pj_count].insn        = insn;
			pj[pj_count].label       = ir->label;
			pj[pj_count].is_cond     = 1;
			pj[pj_count].true_branch = (ir->relop == IR_NE);
			pj_count++;
			break;
		}

		case IR_GOTO: {
			int insn;

			insn = bpf_builder_emit(b,
				(struct bpf_insn)BPF_JUMP(BPF_JMP | BPF_JA,
							  0, 0, 0));

			if (pj_count >= pj_cap) {
				pj_cap = pj_cap ? pj_cap * 2 : 16;
				pj = realloc(pj,
					     pj_cap * sizeof(*pj));
				if (!pj)
					abort();
			}

			pj[pj_count].insn        = insn;
			pj[pj_count].label       = ir->label;
			pj[pj_count].is_cond     = 0;
			pj[pj_count].true_branch = 0;
			pj_count++;
			break;
		}

		case IR_LABEL:
			label_pc[ir->label] = bpf_builder_count(b);
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