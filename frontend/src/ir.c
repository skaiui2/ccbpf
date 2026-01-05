#include "ir.h"
#include <stdio.h>
#include <stdint.h>

#define IR_POOL_SIZE 2048

static struct IR ir_pool[IR_POOL_SIZE];
static int ir_count = 0;

struct IR *ir_head = NULL;
struct IR *ir_tail = NULL;

void ir_emit(struct IR ir)
{
    /* print IR*/
    switch (ir.op) {
    case IR_MOVE:
        printf("[IR] MOVE  t%d <- %d\n", ir.dst, ir.src1);
        break;

    case IR_ADD:
        printf("[IR] ADD   t%d <- t%d + t%d\n", ir.dst, ir.src1, ir.src2);
        break;

    case IR_SUB:
        printf("[IR] SUB   t%d <- t%d - t%d\n", ir.dst, ir.src1, ir.src2);
        break;

    case IR_MUL:
        printf("[IR] MUL   t%d <- t%d * t%d\n", ir.dst, ir.src1, ir.src2);
        break;

    case IR_DIV:
        printf("[IR] DIV   t%d <- t%d / t%d\n", ir.dst, ir.src1, ir.src2);
        break;

    case IR_LOAD:
        printf("[IR] LOAD  t%d <- MEM[%d + t%d * %d]\n",
               ir.dst, ir.array_base, ir.array_index, ir.array_width);
        break;

    case IR_STORE:
        printf("[IR] STORE MEM[%d + t%d * %d] <- t%d\n",
               ir.array_base, ir.array_index, ir.array_width, ir.src1);
        break;

    case IR_IF_FALSE:
        printf("[IR] IFFALSE (t%d %s t%d) goto L%d\n",
               ir.src1,
               (ir.relop == IR_LT ? "<" :
                ir.relop == IR_LE ? "<=" :
                ir.relop == IR_GT ? ">" :
                ir.relop == IR_GE ? ">=" :
                ir.relop == IR_EQ ? "==" :
                ir.relop == IR_NE ? "!=" : "?"),
               ir.src2,
               ir.label);
        break;

    case IR_GOTO:
        printf("[IR] GOTO L%d\n", ir.label);
        break;

    case IR_LABEL:
        printf("[IR] LABEL L%d\n", ir.label);
        break;

    default:
        printf("[IR] ???\n");
        break;
    }

}
