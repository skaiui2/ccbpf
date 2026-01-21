#ifndef IR_H
#define IR_H
#include <stdarg.h>

enum IR_Op {
    IR_NOP = 0,

    IR_MOVE,

    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,

    IR_AND,
    IR_OR,

    IR_RET,
    IR_LOAD_CTX,    
    IR_LOAD_PKT,

    IR_LOAD,

    IR_RELOP, 

    IR_NEG, 
    IR_NOT, 

    IR_STORE,

    IR_IF_FALSE,

    IR_GOTO,

    IR_LABEL,
};

enum IR_RelOp {
    IR_GT,  
    IR_GE, 
    IR_EQ,
    IR_NE,  
};

struct IR {
    enum IR_Op op;

    int dst;   
    int src1;   
    int src2;  

    int array_base;   
    int array_index; 
    int array_width;  

    enum IR_RelOp relop;

    int label;

    struct IR *next;
};



extern struct IR *ir_head;
extern struct IR *ir_tail;

void ir_emit(struct IR ir);
void ir_parse_and_emit(const char *fmt, va_list ap);

#endif
