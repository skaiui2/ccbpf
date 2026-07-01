#ifndef INTER_H
#define INTER_H

#include "lexer.h"
#include "ir.h"
#include "symbols.h"
#include <stddef.h>

void init_constant_singletons(void);
void init_stmt_singletons(void);
char *region_strdup(const char *s);

#define STRING_POOL_SIZE  128
extern char *string_pool[STRING_POOL_SIZE];
extern int   string_pool_count;

void string_count_stats();

enum NodeTag {
    TAG_NODE = 0,

    TAG_ID,
    TAG_CONSTANT,
    TAG_ACCESS,
    TAG_ARITH,
    TAG_UNARY,
    TAG_REL,
    TAG_STRING,
    TAG_LOGICAL,

    TAG_RETURN,
    TAG_CTX,
    TAG_CTX_PTR,

    TAG_BUILTIN_CALL,

    TAG_SET,
    TAG_SETELEM,
    TAG_IF,
    TAG_ELSE,
    TAG_SEQ,
};

struct Node {
    int  lexline;
    enum NodeTag tag;
    void (*gen)(struct Node *self, int b, int a);
    void (*jumping)(struct Node *self, int t, int f);
    char *(*tostring)(struct Node *self);
};

struct Node *node_new(void);
void         node_error(struct Node *self, const char *msg);
int          node_newlabel(void);
void         node_emitlabel(int i);
void         node_emit(const char *fmt, ...);

struct Expr {
    struct Node       base;
    struct lexer_token *op;
    struct Type       *type;
    int temp_no;
};

struct Expr *expr_new(struct lexer_token *tok, struct Type *type);

struct CtxExpr {
    struct Expr base;
    int offset;
};

struct Expr *ctx_load_expr_new(int offset);

struct BuiltinCall {
    struct Expr base;
    const char *name;    
    int         native_id;
    int         argc;
    struct Expr *args[4];
};

struct BuiltinCall *builtin_call_new(const char *name,
                                     int native_id,
                                     int argc,
                                     struct Expr **args);

struct Stmt {
    struct Node base;
    int         after;
};

extern struct Stmt *Stmt_Null;
extern struct Stmt *Stmt_Enclosing;

struct Stmt *stmt_new(void);

struct CtxPtrExpr {
    struct Expr base;
    int base_offset;
    struct StructType *st;
};

struct CtxPtrExpr *ctx_ptr_new(int base_offset, struct Type *ty);

struct Return {
    struct Stmt base;
    struct Expr *expr;
};

struct Return *return_new(struct Expr *expr);

struct Op {
    struct Expr base;
};

struct Op *op_new(struct lexer_token *tok, struct Type *type);

struct Logical {
    struct Expr base;
    struct Expr *e1;
    struct Expr *e2;
    int temp_no;
};

struct Logical *logical_new(struct lexer_token *tok, struct Expr *e1, struct Expr *e2);

struct Access {
    struct Op  base;
    struct Expr *array;
    struct Expr *index;
    int slot;
    int width;
};

struct Access *access_new(struct Expr *array, struct Expr *index, struct Type *type);

struct And {
    struct Logical base;
};

struct And *and_new(struct lexer_token *tok, struct Expr *e1, struct Expr *e2);

struct Arith {
    struct Op  base;
    struct Expr *e1;
    struct Expr *e2;
};

struct Arith *arith_new(struct lexer_token *tok, struct Expr *e1, struct Expr *e2);

struct BitAnd {
    struct Op base;
    struct Expr *e1;
    struct Expr *e2;
};

struct BitAnd *bitand_new(struct lexer_token *tok, struct Expr *e1, struct Expr *e2);

struct BitOr {
    struct Op base;
    struct Expr *e1;
    struct Expr *e2;
};

struct BitOr *bitor_new(struct lexer_token *tok, struct Expr *e1, struct Expr *e2);

struct Constant {
    struct Expr base;
    int int_val;
};

struct Constant *constant_new(struct lexer_token *tok, struct Type *type);
struct Constant *constant_int(int value);

extern struct Constant *Constant_true;
extern struct Constant *Constant_false;

struct Else {
    struct Stmt base;
    struct Expr *expr;
    struct Stmt *stmt1;
    struct Stmt *stmt2;
};

struct Else *else_new(struct Expr *expr, struct Stmt *stmt1, struct Stmt *stmt2);

struct Id {
    struct Expr base;
    int         offset;
    int         base_offset;
    struct StructType *st;
    int is_ctx_ptr;
};

struct Id *id_new(struct lexer_token *word, struct Type *type, int offset);
struct Id *id_new_from_name(const char *name, struct Type *ty, int offset);

struct If {
    struct Stmt base;
    struct Expr *expr;
    struct Stmt *stmt;
};

struct If *if_new(struct Expr *expr, struct Stmt *stmt);

struct Not {
    struct Logical base;
};

struct Not *not_new(struct lexer_token *tok, struct Expr *x2);

struct Or {
    struct Logical base;
};

struct Or *or_new(struct lexer_token *tok, struct Expr *x1, struct Expr *x2);

enum AST_RelOp {
    AST_LT,
    AST_LE,
    AST_GT,
    AST_GE,
    AST_EQ,
    AST_NE,
};

struct Rel {
    struct Logical base;
    enum AST_RelOp relop;
};

struct Rel *rel_new(struct lexer_token *tok, struct Expr *x1, struct Expr *x2);

struct StringLiteral {
    struct Expr base;
    int str_id;
};

struct Expr *string_literal_new(const char *s);

struct Seq {
    struct Stmt base;
    struct Stmt *s1;
    struct Stmt *s2;
};

struct Seq *seq_new(struct Stmt *s1, struct Stmt *s2);

struct Set {
    struct Stmt base;
    struct Id   *id;
    struct Expr *expr;
};

struct Set *set_new(struct Id *id, struct Expr *expr);

struct SetElem {
    struct Stmt base;
    struct Id   *array;
    struct Expr *index;
    struct Expr *expr;
    int slot;
    int width;
};

struct SetElem *setelem_new(struct Access *x, struct Expr *y);

struct Unary {
    struct Op  base;
    struct Expr *expr;
};

struct Unary *unary_new(struct lexer_token *tok, struct Expr *expr);

extern struct Type *Type_Int;
extern struct Type *Type_Bool;

#endif
