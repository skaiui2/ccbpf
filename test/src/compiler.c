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

int compiler_test(void)
{
    ir_init();  

    const char *filename = "../hello.c";

    struct lexer lex;
    lexer_init(&lex);
    lexer_set_input(filename);

    struct Parser *p = parser_new(&lex);
    if (!p) {
        fprintf(stderr, "failed to create parser\n");
        return 1;
    }

    /* parser and generate IR*/
    parser_program(p);

    /* IR -> BPF */
    struct bpf_builder b;
    bpf_builder_init(&b);

    struct ir_mes im;
    ir_mes_get(&im);

    ir_lower_program(im.ir_head, im.label_count, &b);

    struct bpf_insn *prog = bpf_builder_data(&b);
    int prog_len = bpf_builder_count(&b);

    write_ccbpf("out.ccbpf", prog, prog_len);
    printf("Wrote out.ccbpf (%d instructions)\n", prog_len);

    bpf_builder_free(&b);
    return 0;
}

int main(void)
{
    compiler_test();  

    return 0;
}
