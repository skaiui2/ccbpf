/* include/inter.h */

#ifndef INTER_H
#define INTER_H

#include "lexer.h"
#include "ir.h"
#include "symbols.h"
#include <stddef.h>

/* ===== Node ===== */
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
    TAG_WHILE,
    TAG_DO,
    TAG_BREAK,
    TAG_SEQ,
    TAG_BLOCK,
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



/* ===== Expr ===== */

struct Expr {
    struct Node       base;
    struct lexer_token *op;
    struct Type       *type;
    int temp_no;
};

struct Expr *expr_new(struct lexer_token *tok, struct Type *type);

/* ========= ctx ==========*/
struct CtxExpr {
    struct Expr base;
    int offset;   // offsete on ctx 
};

struct Expr *ctx_load_expr_new(int offset);

/* BUILT*/
struct BuiltinCall {
    struct Expr base;
    int func_id;           /* NATIVE_NTOHL / NATIVE_PRINTF  */
    int argc;
    struct Expr *args[4]; 
};

struct BuiltinCall *builtin_call_new(int func_id, int argc, struct Expr **args);

/* ===== Stmt ===== */

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
/* ===== Return ===== */
struct Return {
    struct Stmt base;
    struct Expr *expr;
};

struct Return *return_new(struct Expr *expr);


/* ===== Op ===== */

struct Op {
    struct Expr base;
};

struct Op *op_new(struct lexer_token *tok, struct Type *type);

/* ===== Logical ===== */

struct Logical {
    struct Expr base;
    struct Expr *e1;
    struct Expr *e2;
    int temp_no; 
};

struct Logical *logical_new(struct lexer_token *tok, struct Expr *e1, struct Expr *e2);

/* ===== Access ===== */

struct Access {
    struct Op  base;
    struct Expr *array;
    struct Expr *index;

    int slot; 
    int width;
};

struct Access *access_new(struct Expr *array, struct Expr *index, struct Type *type);

/* ===== And ===== */

struct And {
    struct Logical base;
};

struct And *and_new(struct lexer_token *tok, struct Expr *e1, struct Expr *e2);

/* ===== Arith ===== */

struct Arith {
    struct Op  base;
    struct Expr *e1;
    struct Expr *e2;
};

struct Arith *arith_new(struct lexer_token *tok, struct Expr *e1, struct Expr *e2);

/* ===== BitAnd ===== */

struct BitAnd {
    struct Op base;
    struct Expr *e1;
    struct Expr *e2;
};

struct BitAnd *bitand_new(struct lexer_token *tok, struct Expr *e1, struct Expr *e2);

/* ===== BitOr ===== */

struct BitOr {
    struct Op base;
    struct Expr *e1;
    struct Expr *e2;
};

struct BitOr *bitor_new(struct lexer_token *tok, struct Expr *e1, struct Expr *e2);

/* ===== Break ===== */

struct Break {
    struct Stmt  base;
    struct Stmt *stmt;
};

struct Break *break_new(void);

/* ===== Constant ===== */

struct Constant {
    struct Expr base;   
    int int_val;
    float real_val;
};

struct Constant *constant_new(struct lexer_token *tok, struct Type *type);
struct Constant *constant_int(int value);
struct Constant *constant_float(float v);

extern struct Constant *Constant_true;
extern struct Constant *Constant_false;

/* ===== Do ===== */

struct Do {
    struct Stmt base;
    struct Stmt *stmt;
    struct Expr *expr;
};

struct Do *do_new(void);
void       do_init(struct Do *d, struct Stmt *s, struct Expr *x);

/* ===== Else ===== */

struct Else {
    struct Stmt base;
    struct Expr *expr;
    struct Stmt *stmt1;
    struct Stmt *stmt2;
};

struct Else *else_new(struct Expr *expr, struct Stmt *stmt1, struct Stmt *stmt2);

/* ===== For ===== */

struct For {
    struct Stmt base;
    struct Stmt *init;
    struct Expr *cond;
    struct Stmt *step;
    struct Stmt *body;
};

struct For *for_new(struct Stmt *init, struct Expr *cond, struct Stmt *step, struct Stmt *body);

/* ===== Id ===== */

struct Id {
    struct Expr base;
    int         offset;
    int         base_offset;
    struct StructType *st;

    int is_ctx_ptr;  
};

struct Id *id_new(struct lexer_token *word, struct Type *type, int offset);

struct Id *id_new_from_name(const char *name, struct Type *ty, int offset);
/* ===== If ===== */

struct If {
    struct Stmt base;
    struct Expr *expr;
    struct Stmt *stmt;
};

struct If *if_new(struct Expr *expr, struct Stmt *stmt);

/* ===== Not ===== */

struct Not {
    struct Logical base;
};

struct Not *not_new(struct lexer_token *tok, struct Expr *x2);

/* ===== Or ===== */

struct Or {
    struct Logical base;
};

struct Or *or_new(struct lexer_token *tok, struct Expr *x1, struct Expr *x2);

/* ===== Rel ===== */
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


/*STRING*/
struct StringLiteral {
    struct Expr base;
    int str_id;   
};

struct Expr *string_literal_new(const char *s); 
/* ===== Seq ===== */

struct Seq {
    struct Stmt base;
    struct Stmt *s1;
    struct Stmt *s2;
};

struct Seq *seq_new(struct Stmt *s1, struct Stmt *s2);

/* ===== Set ===== */

struct Set {
    struct Stmt base;
    struct Id   *id;
    struct Expr *expr;
};

struct Set *set_new(struct Id *id, struct Expr *expr);

/* ===== SetElem ===== */

struct SetElem {
    struct Stmt base;
    struct Id   *array;
    struct Expr *index;
    struct Expr *expr;

    int slot; 
    int width;
};

struct SetElem *setelem_new(struct Access *x, struct Expr *y);

/* ===== Temp ===== */

struct Temp {
    struct Expr base;
    int         number;
};

struct Temp *temp_new(struct Type *type);

/* ===== Unary ===== */

struct Unary {
    struct Op  base;
    struct Expr *expr;
};

struct Unary *unary_new(struct lexer_token *tok, struct Expr *expr);

/* ===== While ===== */

struct While {
    struct Stmt base;
    struct Expr *expr;
    struct Stmt *stmt;
};

struct While *while_new(void);
void          while_init(struct While *w, struct Expr *expr, struct Stmt *stmt);

/* ===== Common global types (used by parser/inter) ===== */

extern struct Type *Type_Int;
extern struct Type *Type_Float;
extern struct Type *Type_Bool;

#endif
