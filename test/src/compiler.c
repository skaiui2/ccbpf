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


/* ===== 清空 IR 池 ===== */
extern struct IR *ir_head;
extern struct IR *ir_tail;
extern int ir_count;   // 你需要在 ir.c 里把 ir_count 改成非 static

static void ir_reset(void)
{
    ir_head = NULL;
    ir_tail = NULL;
    ir_count = 0;
}

/* ===== 编译 hello.c → out.ccbpf ===== */
int compiler_test(void)
{
    ir_reset();   // ★ 清空旧 IR

    const char *filename = "../hello.c";

    struct lexer lex;
    lexer_init(&lex);
    lexer_set_input(filename);

    struct Parser *p = parser_new(&lex);
    if (!p) {
        fprintf(stderr, "failed to create parser\n");
        return 1;
    }

    /* 1. 解析 + 生成 IR */
    parser_program(p);

    /* 2. IR → BPF */
    struct bpf_builder b;
    bpf_builder_init(&b);

    int label_count = 0;
    for (struct IR *x = ir_head; x; x = x->next)
        if (x->label > label_count)
            label_count = x->label;
    label_count++;

    ir_lower_program(ir_head, label_count, &b);

    /* 3. 写出 out.ccbpf */
    struct bpf_insn *prog = bpf_builder_data(&b);
    int prog_len = bpf_builder_count(&b);

    write_ccbpf("out.ccbpf", prog, prog_len);
    printf("Wrote out.ccbpf (%d instructions)\n", prog_len);

    bpf_builder_free(&b);
    return 0;
}

/* ===== hook 测试 ===== */
int test_add(int a, int b)
{
    struct hook_ctx ctx = {
        .arg0 = a,
        .arg1 = b,
    };

    struct ccbpf_program prog = ccbpf_load("out.ccbpf");

    uint32_t r = ccbpf_run_ctx(&prog, &ctx, sizeof(ctx));

    ccbpf_unload(&prog);

    return a + b + r;
}

int test_pkt(uint8_t *packet, int len)
{
    struct hook_ctx ctx = { .arg0 = 0, .arg1 = 0 };

    struct ccbpf_program prog = ccbpf_load("out.ccbpf");

    uint32_t r = ccbpf_run_ctx(&prog, packet, len);

    ccbpf_unload(&prog);

    return r;
}

/* ===== main ===== */
int main(void)
{
    compiler_test();   // 编译 hello.c → out.ccbpf
    
    uint8_t buf[64] = {0};
    buf[23] = 6;   // TCP

    printf("result = %d\n", test_pkt(buf, 64));   // expect PASS


    return 0;
}
