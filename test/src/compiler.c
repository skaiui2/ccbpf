#include<stdio.h>
#include<stdlib.h>
#include<memory.h>
#include<unistd.h>
#include<fcntl.h>
#include "lexer.h"
#include "parser.h"
#include "ir.h"
#include "bpf_builder.h"
#include "cbpf.h"
#include "ir_lowering.h"
#include "ccbpf.h"

void bpf_vm_code_test()
{
    struct bpf_insn prog[] = {
        // mem[0] = 10
        { BPF_LD|BPF_IMM, 0,0,10 },
        { BPF_ST,         0,0,0 },

        // mem[1] = 20
        { BPF_LD|BPF_IMM, 0,0,20 },
        { BPF_ST,         0,0,1 },

        // mem[2] = 30
        { BPF_LD|BPF_IMM, 0,0,30 },
        { BPF_ST,         0,0,2 },

        // mem[3] = 40
        { BPF_LD|BPF_IMM, 0,0,40 },
        { BPF_ST,         0,0,3 },

        // mem[4] = 50
        { BPF_LD|BPF_IMM, 0,0,50 },
        { BPF_ST,         0,0,4 },

        // A = mem[0]
        { BPF_LD|BPF_MEM, 0,0,0 },

        // A += mem[1]
        { BPF_LDX|BPF_MEM, 0,0,1 },
        { BPF_ALU|BPF_ADD|BPF_X, 0,0,0 },

        // A += mem[2]
        { BPF_LDX|BPF_MEM, 0,0,2 },
        { BPF_ALU|BPF_ADD|BPF_X, 0,0,0 },

        // A += mem[3]
        { BPF_LDX|BPF_MEM, 0,0,3 },
        { BPF_ALU|BPF_ADD|BPF_X, 0,0,0 },

        // A += mem[4]
        { BPF_LDX|BPF_MEM, 0,0,4 },
        { BPF_ALU|BPF_ADD|BPF_X, 0,0,0 },

        // return A
        { BPF_RET|BPF_A, 0,0,0 },
    };

    uint8_t dummy[1] = {0};

    u_int result = bpf_filter(prog, dummy, sizeof(dummy), sizeof(dummy));

    printf("BPF sum result: %u\n", result); // 预期：150
}

static int compute_label_count(struct IR *head)
{
    int max = -1;
    struct IR *p;

    for (p = head; p; p = p->next) {
        if ((p->op == IR_LABEL ||
             p->op == IR_IF_FALSE ||
             p->op == IR_GOTO) &&
            p->label > max)
            max = p->label;
    }

    return max + 1;
}

int main(int argc, char **argv)
{
    const char *filename = "../hello.c";
    if (argc > 1)
        filename = argv[1];

    struct lexer lex;
    lexer_init(&lex);
    lexer_set_input(filename);

    struct Parser *p = parser_new(&lex);
    if (!p) {
        fprintf(stderr, "failed to create parser\n");
        return 1;
    }

    parser_program(p);
    bpf_vm_code_test();

    struct bpf_builder b;
    bpf_builder_init(&b);

    int label_count = compute_label_count(ir_head);

    ir_lower_program(ir_head, label_count, &b);

    bpf_builder_emit(&b,
        (struct bpf_insn)BPF_STMT(BPF_LD | BPF_MEM, MEM_C));
    bpf_builder_emit(&b,
        (struct bpf_insn)BPF_STMT(BPF_RET | BPF_A, 0));

    struct bpf_insn *prog = bpf_builder_data(&b);
    int prog_len = bpf_builder_count(&b);

    write_ccbpf("out.ccbpf", prog, prog_len); 
    printf("Wrote out.ccbpf (%d instructions)\n", prog_len);

    struct ccbpf_program ccbpf_prog = ccbpf_load("out.ccbpf");

    u_int ret = ccbpf_run(&ccbpf_prog);
    ccbpf_unload(&ccbpf_prog);

    printf("BPF VM result = %u\n", ret);

    bpf_builder_free(&b);

    return 0;
}
