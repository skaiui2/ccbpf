#include "inter.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static struct Type TYPE_INT_OBJ   = { TYPE_INT,   4 };
static struct Type TYPE_FLOAT_OBJ = { TYPE_FLOAT, 8 };
static struct Type TYPE_BOOL_OBJ  = { TYPE_BOOL,  1 };

struct Type *Type_Int   = &TYPE_INT_OBJ;
struct Type *Type_Float = &TYPE_FLOAT_OBJ;
struct Type *Type_Bool  = &TYPE_BOOL_OBJ;

/* ============================================================
 *  工具：类型提升
 * ============================================================ */

static struct Type *type_max(struct Type *a, struct Type *b)
{
    if (!a || !b) return NULL;

    if (a->tag == TYPE_FLOAT || b->tag == TYPE_FLOAT)
        return Type_Float;
    if (a->tag == TYPE_INT || b->tag == TYPE_INT)
        return Type_Int;
    if (a->tag == TYPE_CHAR && b->tag == TYPE_CHAR)
        return a;

    return NULL;
}

/* ============================================================
 *  token -> string
 * ============================================================ */

static char *token_to_string(struct lexer_token *tok)
{
    if (!tok) return strdup("<?>");

    if (tok->lexeme)
        return strdup(tok->lexeme);

    if (tok->tag == NUM) {
        char *buf = malloc(32);
        snprintf(buf, 32, "%d", tok->int_val);
        return buf;
    }

    if (tok->tag == REAL) {
        char *buf = malloc(32);
        snprintf(buf, 32, "%f", tok->real_val);
        return buf;
    }

    // 显式处理所有运算符 / 关键符号
    switch (tok->tag) {
    case AND_BIT:    return strdup("&");
    case OR_BIT:     return strdup("|");
    case LT:         return strdup("<");
    case GT:         return strdup(">");
    case PLUS:       return strdup("+");
    case MINUS:      return strdup("-");
    case STAR:       return strdup("*");
    case SLASH:      return strdup("/");
    case MOD:        return strdup("%");
    case ASSIGN:     return strdup("=");
    case ADD_ASSIGN: return strdup("+=");
    case SUB_ASSIGN: return strdup("-=");
    case MUL_ASSIGN: return strdup("*=");
    case DIV_ASSIGN: return strdup("/=");
    case INC:        return strdup("++");
    case DEC:        return strdup("--");
    case EQ:         return strdup("==");
    case NE:         return strdup("!=");
    case LE:         return strdup("<=");
    case GE:         return strdup(">=");
    case AND:        return strdup("&&");
    case OR:         return strdup("||");
    case LPAREN:     return strdup("(");
    case RPAREN:     return strdup(")");
    case LBRACE:     return strdup("{");
    case RBRACE:     return strdup("}");
    case LBRACKET:   return strdup("[");
    case RBRACKET:   return strdup("]");
    case COMMA:      return strdup(",");
    case SEMICOLON:  return strdup(";");
    case DOT:        return strdup(".");
    default: {
        char *buf = malloc(16);
        snprintf(buf, 16, "#%d", tok->tag);
        return buf;
    }
    }
}

/* ============================================================
 *  Node
 * ============================================================ */

static int g_labels = 0;

struct Node *node_new(void)
{
    struct Node *n = malloc(sizeof(struct Node));
    n->lexline  = 0;
    n->gen      = NULL;
    n->jumping  = NULL;
    n->tostring = NULL;
    return n;
}

void node_error(struct Node *self, const char *msg)
{
    fprintf(stderr, "near line %d: %s\n",
            self ? self->lexline : -1, msg);
    exit(1);
}

int node_newlabel(void)
{
    return ++g_labels;
}

void node_emitlabel(int i)
{
    printf("L%d:\n", i);
}

void node_emit(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("\t");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}

static void node_emit_jumps(const char *test, int t, int f)
{
    if (t != 0 && f != 0) {
        node_emit("if %s goto L%d", test, t);
        node_emit("goto L%d", f);
    } else if (t != 0) {
        node_emit("if %s goto L%d", test, t);
    } else if (f != 0) {
        node_emit("iffalse %s goto L%d", test, f);
    }
}

/* ============================================================
 *  Expr
 * ============================================================ */

static struct Node *expr_gen(struct Node *self);
static void         expr_jumping(struct Node *self, int t, int f);
static char        *expr_tostring(struct Node *self);

struct Expr *expr_new(struct lexer_token *tok, struct Type *type)
{
    struct Expr *e = malloc(sizeof(struct Expr));
    e->op   = tok;
    e->type = type;

    e->base.gen      = (void *)expr_gen;
    e->base.jumping  = expr_jumping;
    e->base.tostring = expr_tostring;

    return e;
}

static struct Node *expr_gen(struct Node *self)
{
    return self;
}

static void expr_jumping(struct Node *self, int t, int f)
{
    char *s = self->tostring(self);
    node_emit_jumps(s, t, f);
}

static char *expr_tostring(struct Node *self)
{
    struct Expr *e = (struct Expr *)self;
    return token_to_string(e->op);
}

/* ============================================================
 *  Stmt
 * ============================================================ */

struct Stmt *Stmt_Null      = NULL;
struct Stmt *Stmt_Enclosing = NULL;

struct Stmt *stmt_new(void)
{
    struct Stmt *s = malloc(sizeof(struct Stmt));
    s->after = 0;
    s->base.gen      = NULL;
    s->base.jumping  = NULL;
    s->base.tostring = NULL;
    return s;
}

__attribute__((constructor))
static void init_stmt_singletons(void)
{
    Stmt_Null      = stmt_new();
    Stmt_Enclosing = Stmt_Null;
}

/* ============================================================
 *  Constant
 * ============================================================ */

struct Constant *Constant_true  = NULL;
struct Constant *Constant_false = NULL;

static char *constant_tostring(struct Node *self);
static void  constant_jumping(struct Node *self, int t, int f);

struct Constant *constant_new(struct lexer_token *tok, struct Type *type)
{
    struct Constant *c = malloc(sizeof(struct Constant));

    c->base.op   = tok;
    c->base.type = type;

    c->base.base.gen      = (void *)expr_gen;
    c->base.base.jumping  = constant_jumping;
    c->base.base.tostring = constant_tostring;

    return c;
}

struct Constant *constant_int(int value)
{
    struct lexer_token *tok = malloc(sizeof(struct lexer_token));
    tok->tag     = NUM;
    tok->int_val = value;
    tok->lexeme  = NULL;
    return constant_new(tok, Type_Int);
}

struct Constant *constant_float(float v)
{
    struct lexer_token *tok = malloc(sizeof(struct lexer_token));
    tok->tag      = REAL;
    tok->real_val = v;
    tok->lexeme   = NULL;

    struct Constant *c = malloc(sizeof(struct Constant));

    c->base.op   = tok;         
    c->base.type = Type_Float;   
    c->real_val  = v;

    c->base.base.gen      = (void *)expr_gen;         
    c->base.base.jumping  = constant_jumping; 
    c->base.base.tostring = constant_tostring; 

    return c;
}

__attribute__((constructor))
static void init_constant_singletons(void)
{
    static struct lexer_token tok_true  = { TRUE,  0, .lexeme = "true" };
    static struct lexer_token tok_false = { FALSE, 0, .lexeme = "false" };

    Constant_true  = constant_new(&tok_true,  Type_Bool);
    Constant_false = constant_new(&tok_false, Type_Bool);
}

static char *constant_tostring(struct Node *self)
{
    struct Constant *c = (struct Constant *)self;
    return token_to_string(c->base.op);
}

static void constant_jumping(struct Node *self, int t, int f)
{
    struct Constant *c = (struct Constant *)self;

    if (c == Constant_true && t != 0) {
        node_emit("goto L%d", t);
    } else if (c == Constant_false && f != 0) {
        node_emit("goto L%d", f);
    } else {
        char *s = self->tostring(self);
        node_emit_jumps(s, t, f);
    }
}

/* ============================================================
 *  Op
 * ============================================================ */

static char *op_tostring(struct Node *self);

struct Op *op_new(struct lexer_token *tok, struct Type *type)
{
    struct Op *o = malloc(sizeof(struct Op));
    o->base.op   = tok;
    o->base.type = type;

    o->base.base.gen      = (void *)expr_gen;
    o->base.base.jumping  = expr_jumping;
    o->base.base.tostring = op_tostring;

    return o;
}

static char *op_tostring(struct Node *self)
{
    struct Op *o = (struct Op *)self;
    return token_to_string(o->base.op);
}

/* ============================================================
 *  Arith
 * ============================================================ */

static char *arith_tostring(struct Node *self);

struct Arith *arith_new(struct lexer_token *tok, struct Expr *e1, struct Expr *e2)
{
    struct Arith *a = malloc(sizeof(struct Arith));

    a->base.base.op   = tok;
    a->base.base.type = type_max(e1->type, e2->type);

    if (!a->base.base.type)
        node_error((struct Node *)a, "type error");

    a->e1 = e1;
    a->e2 = e2;

    a->base.base.base.gen      = (void *)expr_gen;
    a->base.base.base.jumping  = expr_jumping;
    a->base.base.base.tostring = arith_tostring;

    return a;
}

static char *arith_tostring(struct Node *self)
{
    struct Arith *a = (struct Arith *)self;

    char *s1 = a->e1->base.tostring((struct Node *)a->e1);
    char *s2 = a->e2->base.tostring((struct Node *)a->e2);
    char *op = token_to_string(a->base.base.op);

    size_t len = strlen(s1) + strlen(op) + strlen(s2) + 10;
    char *buf = malloc(len);
    snprintf(buf, len, "%s %s %s", s1, op, s2);
    return buf;
}

/* ============================================================
 *  BitAnd
 * ============================================================ */

static char *bitand_tostring(struct Node *self);

struct BitAnd *bitand_new(struct lexer_token *tok, struct Expr *e1, struct Expr *e2)
{
    struct BitAnd *b = malloc(sizeof(struct BitAnd)); 

    b->base.base.op   = tok;
    b->base.base.type = type_max(e1->type, e2->type);

    if (!b->base.base.type)
        node_error((struct Node *)b, "type error");

    b->e1 = e1;
    b->e2 = e2;

    b->base.base.base.gen      = (void *)expr_gen;
    b->base.base.base.jumping  = expr_jumping;
    b->base.base.base.tostring = arith_tostring;

    return b;
}

static char *bitand_tostring(struct Node *self)
{
    struct BitAnd *b = (struct BitAnd *)self;

    char *s1 = b->e1->base.tostring((struct Node *)b->e1);
    char *s2 = b->e2->base.tostring((struct Node *)b->e2);
    char *op = token_to_string(b->base.base.op); 

    size_t len = strlen(s1) + strlen(op) + strlen(s2) + 10;
    char *buf = malloc(len);
    snprintf(buf, len, "%s %s %s", s1, op, s2);
    return buf;
}

static char *bitor_tostring(struct Node *self);

struct BitOr *bitor_new(struct lexer_token *tok, struct Expr *e1, struct Expr *e2)
{
    struct BitOr *b = malloc(sizeof(struct BitOr));

    b->base.base.op   = tok;
    b->base.base.type = type_max(e1->type, e2->type);  

    if (!b->base.base.type)
        node_error((struct Node *)b, "type error");

    b->e1 = e1;
    b->e2 = e2;

    b->base.base.base.gen      = (void *)expr_gen;    
    b->base.base.base.jumping  = expr_jumping;      
    b->base.base.base.tostring = bitor_tostring;     

    return b;
}

static char *bitor_tostring(struct Node *self)
{
    struct BitOr *b = (struct BitOr *)self;

    char *s1 = b->e1->base.tostring((struct Node *)b->e1);
    char *s2 = b->e2->base.tostring((struct Node *)b->e2);
    char *op = token_to_string(b->base.base.op);  

    size_t len = strlen(s1) + strlen(op) + strlen(s2) + 10;
    char *buf = malloc(len);
    snprintf(buf, len, "%s %s %s", s1, op, s2);
    return buf;
}

/* ============================================================
 *  Logical（基类）
 * ============================================================ */

static char *logical_tostring(struct Node *self);

struct Logical *logical_new(struct lexer_token *tok, struct Expr *e1, struct Expr *e2)
{
    struct Logical *l = malloc(sizeof(struct Logical));

    l->base.op   = tok;
    l->base.type = Type_Bool;

    l->e1 = e1;
    l->e2 = e2;

    l->base.base.gen      = (void *)expr_gen;
    l->base.base.jumping  = expr_jumping;
    l->base.base.tostring = logical_tostring;

    return l;
}

static char *logical_tostring(struct Node *self)
{
    struct Logical *l = (struct Logical *)self;

    char *s1 = l->e1->base.tostring((struct Node *)l->e1);
    char *s2 = l->e2->base.tostring((struct Node *)l->e2);
    char *op = token_to_string(l->base.op);

    size_t len = strlen(s1) + strlen(op) + strlen(s2) + 10;
    char *buf = malloc(len);
    snprintf(buf, len, "%s %s %s", s1, op, s2);
    return buf;
}

/* ============================================================
 *  And
 * ============================================================ */

static void and_jumping(struct Node *self, int t, int f);

struct And *and_new(struct lexer_token *tok, struct Expr *e1, struct Expr *e2)
{
    struct And *a = malloc(sizeof(struct And));

    a->base.base.op   = tok;
    a->base.base.type = Type_Bool;

    a->base.e1 = e1;
    a->base.e2 = e2;

    a->base.base.base.gen      = (void *)expr_gen;
    a->base.base.base.tostring = logical_tostring;
    a->base.base.base.jumping  = and_jumping;

    return a;
}

static void and_jumping(struct Node *self, int t, int f)
{
    struct And *a = (struct And *)self;

    int label = (f != 0) ? f : node_newlabel();

    a->base.e1->base.jumping((struct Node *)a->base.e1, 0, label);
    a->base.e2->base.jumping((struct Node *)a->base.e2, t, f);

    if (f == 0)
        node_emitlabel(label);
}

/* ============================================================
 *  Or
 * ============================================================ */

static void or_jumping(struct Node *self, int t, int f);

struct Or *or_new(struct lexer_token *tok, struct Expr *e1, struct Expr *e2)
{
    struct Or *o = malloc(sizeof(struct Or));

    o->base.base.op   = tok;
    o->base.base.type = Type_Bool;

    o->base.e1 = e1;
    o->base.e2 = e2;

    o->base.base.base.gen      = (void *)expr_gen;
    o->base.base.base.tostring = logical_tostring;
    o->base.base.base.jumping  = or_jumping;

    return o;
}

static void or_jumping(struct Node *self, int t, int f)
{
    struct Or *o = (struct Or *)self;

    int label = (t != 0) ? t : node_newlabel();

    o->base.e1->base.jumping((struct Node *)o->base.e1, label, 0);
    o->base.e2->base.jumping((struct Node *)o->base.e2, t, f);

    if (t == 0)
        node_emitlabel(label);
}

/* ============================================================
 *  Not
 * ============================================================ */

static void  not_jumping(struct Node *self, int t, int f);
static char *not_tostring(struct Node *self);

struct Not *not_new(struct lexer_token *tok, struct Expr *x)
{
    struct Not *n = malloc(sizeof(struct Not));

    n->base.base.op   = tok;
    n->base.base.type = Type_Bool;

    n->base.e1 = x;
    n->base.e2 = x;

    n->base.base.base.gen      = (void *)expr_gen;
    n->base.base.base.jumping  = not_jumping;
    n->base.base.base.tostring = not_tostring;

    return n;
}

static void not_jumping(struct Node *self, int t, int f)
{
    struct Not *n = (struct Not *)self;
    n->base.e1->base.jumping((struct Node *)n->base.e1, f, t);
}

static char *not_tostring(struct Node *self)
{
    struct Not *n = (struct Not *)self;

    char *op = token_to_string(n->base.base.op);
    char *s  = n->base.e1->base.tostring((struct Node *)n->base.e1);

    size_t len = strlen(op) + strlen(s) + 5;
    char *buf = malloc(len);
    snprintf(buf, len, "%s %s", op, s);
    return buf;
}

/* ============================================================
 *  Rel
 * ============================================================ */

static void rel_jumping(struct Node *self, int t, int f);

struct Rel *rel_new(struct lexer_token *tok, struct Expr *e1, struct Expr *e2)
{
    struct Rel *r = malloc(sizeof(struct Rel));

    r->base.base.op   = tok;
    r->base.base.type = Type_Bool;

    r->base.e1 = e1;
    r->base.e2 = e2;

    r->base.base.base.gen      = (void *)expr_gen;
    r->base.base.base.tostring = logical_tostring;
    r->base.base.base.jumping  = rel_jumping;

    return r;
}

static void rel_jumping(struct Node *self, int t, int f)
{
    struct Rel *r = (struct Rel *)self;

    char *s1 = r->base.e1->base.tostring((struct Node *)r->base.e1);
    char *s2 = r->base.e2->base.tostring((struct Node *)r->base.e2);
    char *op = token_to_string(r->base.base.op);

    size_t len = strlen(s1) + strlen(op) + strlen(s2) + 5;
    char *test = malloc(len);
    snprintf(test, len, "%s %s %s", s1, op, s2);

    node_emit_jumps(test, t, f);
}

/* ============================================================
 *  Access
 * ============================================================ */

static char *access_tostring(struct Node *self);

struct Access *access_new(struct Expr *array, struct Expr *index, struct Type *type)
{
    struct Access *a = malloc(sizeof(struct Access));

    a->base.base.op   = NULL;
    a->base.base.type = type;

    a->array = array;
    a->index = index;

    a->base.base.base.gen      = (void *)expr_gen;
    a->base.base.base.jumping  = expr_jumping;
    a->base.base.base.tostring = access_tostring;

    return a;
}

static char *access_tostring(struct Node *self)
{
    struct Access *a = (struct Access *)self;

    char *arr = a->array->base.tostring((struct Node *)a->array);
    char *idx = a->index->base.tostring((struct Node *)a->index);

    size_t len = strlen(arr) + strlen(idx) + 10;
    char *buf = malloc(len);
    snprintf(buf, len, "%s [ %s ]", arr, idx);
    return buf;
}

/* ============================================================
 *  Id
 * ============================================================ */

static char *id_tostring(struct Node *self);

struct Id *id_new(struct lexer_token *tok, struct Type *type, int offset)
{
    struct Id *i = malloc(sizeof(struct Id));

    i->base.op   = tok;
    i->base.type = type;
    i->offset    = offset;

    i->base.base.gen      = (void *)expr_gen;
    i->base.base.jumping  = expr_jumping;
    i->base.base.tostring = id_tostring;

    return i;
}

static char *id_tostring(struct Node *self)
{
    struct Id *i = (struct Id *)self;
    return token_to_string(i->base.op);
}

/* ============================================================
 *  Seq
 * ============================================================ */

static void seq_gen(struct Node *self, int b, int a);

struct Seq *seq_new(struct Stmt *s1, struct Stmt *s2)
{
    struct Seq *s = malloc(sizeof(struct Seq));
    s->s1 = s1;
    s->s2 = s2;
    s->base.base.gen = seq_gen;
    return s;
}

static void seq_gen(struct Node *self, int b, int a)
{
    struct Seq *s = (struct Seq *)self;

    if (s->s1 == Stmt_Null)
        s->s2->base.gen((struct Node *)s->s2, b, a);
    else if (s->s2 == Stmt_Null)
        s->s1->base.gen((struct Node *)s->s1, b, a);
    else {
        int label = node_newlabel();
        s->s1->base.gen((struct Node *)s->s1, b, label);
        node_emitlabel(label);
        s->s2->base.gen((struct Node *)s->s2, label, a);
    }
}

/* ============================================================
 *  If
 * ============================================================ */

static void if_gen(struct Node *self, int b, int a);

struct If *if_new(struct Expr *expr, struct Stmt *stmt)
{
    struct If *i = malloc(sizeof(struct If));

    i->expr = expr;
    i->stmt = stmt;

    i->base.base.gen = if_gen;

    return i;
}

static void if_gen(struct Node *self, int b, int a)
{
    struct If *i = (struct If *)self;

    int label = node_newlabel();

    i->expr->base.jumping((struct Node *)i->expr, 0, a);
    node_emitlabel(label);
    i->stmt->base.gen((struct Node *)i->stmt, label, a);
}

/* ============================================================
 *  Else
 * ============================================================ */

static void else_gen(struct Node *self, int b, int a);

struct Else *else_new(struct Expr *expr, struct Stmt *s1, struct Stmt *s2)
{
    struct Else *e = malloc(sizeof(struct Else));

    e->expr  = expr;
    e->stmt1 = s1;
    e->stmt2 = s2;

    e->base.base.gen = else_gen;

    return e;
}

static void else_gen(struct Node *self, int b, int a)
{
    struct Else *e = (struct Else *)self;

    int label1 = node_newlabel();
    int label2 = node_newlabel();

    e->expr->base.jumping((struct Node *)e->expr, 0, label2);

    node_emitlabel(label1);
    e->stmt1->base.gen((struct Node *)e->stmt1, label1, a);
    node_emit("goto L%d", a);

    node_emitlabel(label2);
    e->stmt2->base.gen((struct Node *)e->stmt2, label2, a);
}

/* ============================================================
 *  While
 * ============================================================ */

static void while_gen(struct Node *self, int b, int a);

struct While *while_new(void)
{
    struct While *w = malloc(sizeof(struct While));
    w->expr = NULL;
    w->stmt = NULL;
    w->base.base.gen = while_gen;
    return w;
}

void while_init(struct While *w, struct Expr *expr, struct Stmt *stmt)
{
    w->expr = expr;
    w->stmt = stmt;
}

static void while_gen(struct Node *self, int b, int a)
{
    struct While *w = (struct While *)self;

    w->base.after = a;

    int label = node_newlabel();
    node_emitlabel(label);

    w->expr->base.jumping((struct Node *)w->expr, 0, a);
    w->stmt->base.gen((struct Node *)w->stmt, label, a);

    node_emit("goto L%d", label);
}

/* ============================================================
 *  Do
 * ============================================================ */

static void do_gen(struct Node *self, int b, int a);

struct Do *do_new(void)
{
    struct Do *d = malloc(sizeof(struct Do));
    d->stmt = NULL;
    d->expr = NULL;
    d->base.base.gen = do_gen;
    return d;
}

void do_init(struct Do *d, struct Stmt *s, struct Expr *x)
{
    d->stmt = s;
    d->expr = x;
}

static void do_gen(struct Node *self, int b, int a)
{
    struct Do *d = (struct Do *)self;

    d->base.after = a;

    int label = node_newlabel();
    node_emitlabel(label);

    d->stmt->base.gen((struct Node *)d->stmt, label, a);
    d->expr->base.jumping((struct Node *)d->expr, label, 0);
}

/* ============================================================
 *  Break
 * ============================================================ */

static void break_gen(struct Node *self, int b, int a);

struct Break *break_new(void)
{
    struct Break *br = malloc(sizeof(struct Break));

    if (Stmt_Enclosing == Stmt_Null)
        node_error((struct Node *)br, "unenclosed break");

    br->stmt = Stmt_Enclosing;
    br->base.base.gen = break_gen;

    return br;
}

static void break_gen(struct Node *self, int b, int a)
{
    struct Break *br = (struct Break *)self;
    node_emit("goto L%d", br->stmt->after);
}

/* ============================================================
 *  Set
 * ============================================================ */

static void set_gen(struct Node *self, int b, int a);

struct Set *set_new(struct Id *id, struct Expr *expr)
{
    struct Set *s = malloc(sizeof(struct Set));

    s->id   = id;
    s->expr = expr;

    s->base.base.gen = set_gen;

    return s;
}

static void set_gen(struct Node *self, int b, int a)
{
    struct Set *s = (struct Set *)self;

    char *lhs = s->id->base.base.tostring((struct Node *)s->id);
    char *rhs = s->expr->base.tostring((struct Node *)s->expr);

    node_emit("%s = %s", lhs, rhs);
}

/* ============================================================
 *  SetElem
 * ============================================================ */

static void setelem_gen(struct Node *self, int b, int a);

struct SetElem *setelem_new(struct Access *x, struct Expr *y)
{
    struct SetElem *s = malloc(sizeof(struct SetElem));

    s->array = (struct Id *)x->array;
    s->index = x->index;
    s->expr  = y;

    s->base.base.gen = setelem_gen;

    return s;
}

static void setelem_gen(struct Node *self, int b, int a)
{
    struct SetElem *s = (struct SetElem *)self;

    char *arr = s->array->base.base.tostring((struct Node *)s->array);
    char *idx = s->index->base.tostring((struct Node *)s->index);
    char *val = s->expr->base.tostring((struct Node *)s->expr);

    node_emit("%s [ %s ] = %s", arr, idx, val);
}

/* ============================================================
 *  Temp
 * ============================================================ */

static int temp_count = 0;

static char *temp_tostring(struct Node *self)
{
    struct Temp *t = (struct Temp *)self;
    char *buf = malloc(32);
    snprintf(buf, 32, "t%d", t->number);
    return buf;
}

struct Temp *temp_new(struct Type *type)
{
    struct Temp *t = malloc(sizeof(struct Temp));

    t->base.op   = NULL;
    t->base.type = type;
    t->number    = temp_count++;

    t->base.base.gen      = (void *)expr_gen;
    t->base.base.jumping  = expr_jumping;
    t->base.base.tostring = temp_tostring;

    return t;
}

/* ============================================================
 *  Unary
 * ============================================================ */

static char *unary_tostring(struct Node *self);

struct Unary *unary_new(struct lexer_token *tok, struct Expr *expr)
{
    struct Unary *u = malloc(sizeof(struct Unary));

    u->base.base.op   = tok;
    u->base.base.type = type_max(Type_Int, expr->type);

    u->expr = expr;

    u->base.base.base.gen      = (void *)expr_gen;
    u->base.base.base.jumping  = expr_jumping;
    u->base.base.base.tostring = unary_tostring;

    return u;
}

static char *unary_tostring(struct Node *self)
{
    struct Unary *u = (struct Unary *)self;

    char *op = token_to_string(u->base.base.op);
    char *s  = u->expr->base.tostring((struct Node *)u->expr);

    size_t len = strlen(op) + strlen(s) + 5;
    char *buf = malloc(len);
    snprintf(buf, len, "%s %s", op, s);
    return buf;
}
