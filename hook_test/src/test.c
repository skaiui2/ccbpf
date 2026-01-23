#include <stdio.h>
#include <stdint.h>
#include "cbpf.h"
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

    printf("BPF sum result: %u\n", result); // 150
}

int test_pkt(uint8_t *packet, int len)
{
    struct ccbpf_program prog = ccbpf_load("../../build/out.ccbpf");

    uint32_t r = ccbpf_run_pkt(&prog, packet, len);

    ccbpf_unload(&prog);

    return r;
}

int main(void)
{
    
    uint8_t buf[64] = {0};

    buf[34] = 254;
    buf[35] = 0;
    buf[36] = 0;
    buf[37] = 1;

    printf("result = %d\n", test_pkt(buf, 64));  


    return 0;
}