#include "inter.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


struct Stmt *Stmt_Enclosing = NULL;
struct Stmt *Stmt_Null      = NULL;


struct Constant *Constant_true  = NULL;
struct Constant *Constant_false = NULL;


void op_init(Op *op, Token *tok, Type *type)
{
    op->base.tok  = tok;
    op->base.type = type;

    op->base.base.gen      = NULL;
    op->base.base.jumping  = NULL;
    op->base.base.tostring = NULL;
}

void logical_init(Logical *l, Token *tok, Expr *e1, Expr *e2)
{
    l->base.tok  = tok;
    l->base.type = NULL;

    l->e1 = e1;
    l->e2 = e2;

    l->base.base.gen      = NULL;
    l->base.base.jumping  = NULL;
    l->base.base.tostring = NULL;
}


static Expr *access_gen(Node *self);
static void  access_jumping(Node *self, int t, int f);
static char *access_tostring(Node *self);

Access *access_new(Expr *array, Expr *index, Type *type)
{
    Access *a = malloc(sizeof(Access));

    op_init(&a->base, token_new("[]", TAG_INDEX), type);

    a->base.base.base.gen      = access_gen;
    a->base.base.base.jumping  = access_jumping;
    a->base.base.base.tostring = access_tostring;

    a->array = array;
    a->index = index;

    return a;
}

static Expr *access_gen(Node *self)
{
    Access *a = (Access *)self;

    Expr *r = a->index->base.base.gen
        ? (Expr *)a->index->base.base.gen((Node *)a->index)
        : a->index;

    return (Expr *)access_new(a->array, r, a->base.base.type);
}

static void access_jumping(Node *self, int t, int f)
{
    Access *a = (Access *)self;
    char *s = a->base.base.base.tostring((Node *)a);
    emit_jumps(s, t, f);
}

static char *access_tostring(Node *self)
{
    Access *a = (Access *)self;

    char *arr = a->array->base.base.tostring((Node *)a->array);
    char *idx = a->index->base.base.tostring((Node *)a->index);

    size_t len = strlen(arr) + strlen(idx) + 10;
    char *buf = malloc(len);

    snprintf(buf, len, "%s [ %s ]", arr, idx);
    return buf;
}

/* ============================================================
 *                        And
 * ============================================================ */

static void and_jumping(Node *self, int t, int f);

And *and_new(Token *tok, Expr *e1, Expr *e2)
{
    And *a = malloc(sizeof(And));

    logical_init(&a->base, tok, e1, e2);

    a->base.base.base.jumping = and_jumping;

    return a;
}

static void and_jumping(Node *self, int t, int f)
{
    And *a = (And *)self;

    int label = (f != 0) ? f : newlabel();

    a->base.e1->base.base.jumping((Node *)a->base.e1, 0, label);
    a->base.e2->base.base.jumping((Node *)a->base.e2, t, f);

    if (f == 0)
        emitlabel(label);
}

/* ============================================================
 *                        Arith
 * ============================================================ */

static Expr *arith_gen(Node *self);
static char *arith_tostring(Node *self);

Arith *arith_new(Token *tok, Expr *e1, Expr *e2)
{
    Arith *a = malloc(sizeof(Arith));

    op_init(&a->base, tok, NULL);

    a->e1 = e1;
    a->e2 = e2;

    a->base.base.type = type_max(e1->type, e2->type);
    if (!a->base.base.type)
        error("type error");

    a->base.base.base.gen      = arith_gen;
    a->base.base.base.tostring = arith_tostring;

    return a;
}

static Expr *arith_gen(Node *self)
{
    Arith *a = (Arith *)self;

    Expr *r1 = a->e1->base.base.gen
        ? (Expr *)a->e1->base.base.gen((Node *)a->e1)
        : a->e1;

    Expr *r2 = a->e2->base.base.gen
        ? (Expr *)a->e2->base.base.gen((Node *)a->e2)
        : a->e2;

    return (Expr *)arith_new(a->base.base.tok, r1, r2);
}

static char *arith_tostring(Node *self)
{
    Arith *a = (Arith *)self;

    char *s1 = a->e1->base.base.tostring((Node *)a->e1);
    char *op = token_tostring(a->base.base.tok);
    char *s2 = a->e2->base.base.tostring((Node *)a->e2);

    size_t len = strlen(s1) + strlen(op) + strlen(s2) + 10;
    char *buf = malloc(len);

    snprintf(buf, len, "%s %s %s", s1, op, s2);
    return buf;
}

/* ============================================================
 *                        Break
 * ============================================================ */

static void break_gen(Node *self);

Break *break_new()
{
    Break *b = malloc(sizeof(Break));

    if (Stmt_Enclosing == Stmt_Null)
        error("unenclosed break");

    b->stmt = Stmt_Enclosing;

    b->base.base.gen = break_gen;

    return b;
}

static void break_gen(Node *self)
{
    Break *b = (Break *)self;

    char buf[64];
    snprintf(buf, sizeof(buf), "goto L%d", b->stmt->after);
    emit(buf);
}

/* ============================================================
 *                        Constant
 * ============================================================ */

static void constant_jumping(Node *self, int t, int f);

Constant *constant_new(Token *tok, Type *type)
{
    Constant *c = malloc(sizeof(Constant));

    c->base.tok  = tok;
    c->base.type = type;

    c->base.base.jumping = constant_jumping;

    return c;
}

Constant *constant_int(int value)
{
    return constant_new(token_num(value), Type_Int);
}

/* 初始化静态常量 */
__attribute__((constructor))
static void init_constants()
{
    Constant_true  = constant_new(Word_true(),  Type_Bool);
    Constant_false = constant_new(Word_false(), Type_Bool);
}

static void constant_jumping(Node *self, int t, int f)
{
    Constant *c = (Constant *)self;

    if (c == Constant_true && t != 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "goto L%d", t);
        emit(buf);
    }
    else if (c == Constant_false && f != 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "goto L%d", f);
        emit(buf);
    }
}/* ============================================================
 *                        Do
 * ============================================================ */

static void do_gen(Node *self, int b, int a);

Do *do_new()
{
    Do *d = malloc(sizeof(Do));

    d->stmt = NULL;
    d->expr = NULL;

    /* 设置虚函数 */
    d->base.base.gen = do_gen;

    return d;
}

void do_init(Do *d, Stmt *s, Expr *x)
{
    d->stmt = s;
    d->expr = x;

    /* if (mExpr.mType != Type.Bool) error("boolean required in a while"); */
    if (x->type != Type_Bool)
        error("boolean required in a while");
}

static void do_gen(Node *self, int b, int a)
{
    Do *d = (Do *)self;

    /* after = a */
    d->base.after = a;

    /* int label = newlabel(); */
    int label = newlabel();

    /* mStmt.gen(b, label); */
    d->stmt->base.gen((Node *)d->stmt, b, label);

    /* emitlabel(label); */
    emitlabel(label);

    /* mExpr.jumping(b, 0); */
    d->expr->base.base.jumping((Node *)d->expr, b, 0);
}

static void else_gen(Node *self, int b, int a);

Else *else_new(Expr *expr, Stmt *stmt1, Stmt *stmt2)
{
    Else *e = malloc(sizeof(Else));

    e->expr  = expr;
    e->stmt1 = stmt1;
    e->stmt2 = stmt2;

    if (expr->type != Type_Bool)
        error("boolean required in if");

    e->base.base.gen = else_gen;

    return e;
}

static void else_gen(Node *self, int b, int a)
{
    Else *e = (Else *)self;

    int label1 = newlabel();
    int label2 = newlabel();

    e->expr->base.base.jumping((Node *)e->expr, 0, label2);

    emitlabel(label1);
    e->stmt1->base.gen((Node *)e->stmt1, label1, a);
    emit("goto L%d", a);

    emitlabel(label2);
    e->stmt2->base.gen((Node *)e->stmt2, label2, a);
}


static Node *expr_gen(Node *self);
static Node *expr_reduce(Node *self);
static void expr_jumping(Node *self, int t, int f);
static char *expr_tostring(Node *self);

Expr *expr_new(Token *tok, Type *type)
{
    Expr *e = malloc(sizeof(Expr));
    e->op = tok;
    e->type = type;
    e->base.gen = expr_gen;
    e->base.reduce = expr_reduce;
    e->base.jumping = expr_jumping;
    e->base.tostring = expr_tostring;
    return e;
}

static Node *expr_gen(Node *self)
{
    return self;
}

static Node *expr_reduce(Node *self)
{
    return self;
}

static void expr_jumping(Node *self, int t, int f)
{
    char *s = self->tostring(self);
    if (t != 0 && f != 0) {
        emit("if %s goto L%d", s, t);
        emit("goto L%d", f);
    } else if (t != 0) {
        emit("if %s goto L%d", s, t);
    } else if (f != 0) {
        emit("iffalse %s goto L%d", s, f);
    }
}

static char *expr_tostring(Node *self)
{
    Expr *e = (Expr *)self;
    return token_tostring(e->op);
}




static void for_gen(Node *self, int b, int a);

For *for_new(Stmt *init, Expr *cond, Stmt *step, Stmt *body)
{
    For *f = malloc(sizeof(For));

    f->init = init;
    f->cond = cond;
    f->step = step;
    f->body = body;

    if (cond && cond->type != Type_Bool)
        error("boolean required in for");

    f->base.base.gen = for_gen;

    return f;
}

static void for_gen(Node *self, int b, int a)
{
    For *f = (For *)self;

    if (f->init)
        f->init->base.gen((Node *)f->init, b, a);

    int begin = newlabel();
    int step  = newlabel();

    emitlabel(begin);

    if (f->cond)
        f->cond->base.base.jumping((Node *)f->cond, 0, a);

    f->body->base.gen((Node *)f->body, begin, step);

    emitlabel(step);

    if (f->step)
        f->step->base.gen((Node *)f->step, begin, a);

    emit("goto L%d", begin);
}



Id *id_new(Token *word, Type *type, int offset)
{
    Id *i = malloc(sizeof(Id));
    i->base.op   = word;
    i->base.type = type;
    i->offset    = offset;

    i->base.base.gen      = NULL;
    i->base.base.reduce   = NULL;
    i->base.base.jumping  = NULL;
    i->base.base.tostring = NULL;

    return i;
}




static void if_gen(Node *self, int b, int a);

If *if_new(Expr *expr, Stmt *stmt)
{
    If *i = malloc(sizeof(If));

    i->expr = expr;
    i->stmt = stmt;

    if (expr->type != Type_Bool)
        error("boolean required in if");

    i->base.base.gen = if_gen;

    return i;
}

static void if_gen(Node *self, int b, int a)
{
    If *i = (If *)self;

    int label = newlabel();

    i->expr->base.base.jumping((Node *)i->expr, 0, a);

    emitlabel(label);

    i->stmt->base.gen((Node *)i->stmt, label, a);
}




static Node *logical_gen(Node *self);
static char *logical_tostring(Node *self);

Logical *logical_new(Token *tok, Expr *e1, Expr *e2)
{
    Logical *l = malloc(sizeof(Logical));

    l->base.op   = tok;
    l->base.type = NULL;

    l->e1 = e1;
    l->e2 = e2;

    if (e1->type == Type_Bool && e2->type == Type_Bool)
        l->base.type = Type_Bool;
    else
        error("type error");

    l->base.base.gen      = logical_gen;
    l->base.base.tostring = logical_tostring;

    return l;
}

static Node *logical_gen(Node *self)
{
    Logical *l = (Logical *)self;

    int f = newlabel();
    int a = newlabel();

    Temp *t = temp_new(l->base.type);

    self->jumping(self, 0, f);

    emit("%s = true", t->base.base.tostring((Node *)t));
    emit("goto L%d", a);

    emitlabel(f);

    emit("%s = false", t->base.base.tostring((Node *)t));

    emitlabel(a);

    return (Node *)t;
}

static char *logical_tostring(Node *self)
{
    Logical *l = (Logical *)self;

    char *s1 = l->e1->base.base.tostring((Node *)l->e1);
    char *op = token_tostring(l->base.op);
    char *s2 = l->e2->base.base.tostring((Node *)l->e2);

    size_t len = strlen(s1) + strlen(op) + strlen(s2) + 10;
    char *buf = malloc(len);

    snprintf(buf, len, "%s %s %s", s1, op, s2);
    return buf;
}



static int labels = 0;

Node *node_new()
{
    Node *n = malloc(sizeof(Node));
    n->lexline = Lexer_line;
    n->gen = NULL;
    n->jumping = NULL;
    n->tostring = NULL;
    return n;
}

void node_error(Node *self, const char *msg)
{
    fprintf(stderr, "near line %d: %s\n", self->lexline, msg);
    exit(1);
}

int node_newlabel()
{
    return ++labels;
}

void node_emitlabel(int i)
{
    printf("L%d:", i);
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




static void not_jumping(Node *self, int t, int f);
static char *not_tostring(Node *self);

Not *not_new(Token *tok, Expr *x2)
{
    Not *n = malloc(sizeof(Not));

    n->base.base.op   = tok;
    n->base.base.type = Type_Bool;

    n->base.e1 = x2;
    n->base.e2 = x2;

    n->base.base.base.jumping  = not_jumping;
    n->base.base.base.tostring = not_tostring;

    return n;
}

static void not_jumping(Node *self, int t, int f)
{
    Not *n = (Not *)self;
    n->base.e2->base.base.jumping((Node *)n->base.e2, f, t);
}

static char *not_tostring(Node *self)
{
    Not *n = (Not *)self;

    char *op = token_tostring(n->base.base.op);
    char *s2 = n->base.e2->base.base.tostring((Node *)n->base.e2);

    size_t len = strlen(op) + strlen(s2) + 5;
    char *buf = malloc(len);

    snprintf(buf, len, "%s %s", op, s2);
    return buf;
}



static Node *op_reduce(Node *self);

Op *op_new(Token *tok, Type *type)
{
    Op *o = malloc(sizeof(Op));

    o->base.op   = tok;
    o->base.type = type;

    o->base.base.reduce = op_reduce;

    return o;
}

static Node *op_reduce(Node *self)
{
    Expr *expr = (Expr *)self;

    Node *g = expr->base.gen((Node *)expr);

    Temp *t = temp_new(expr->type);

    emit("%s = %s",
         t->base.base.tostring((Node *)t),
         g->tostring(g));

    return (Node *)t;
}




static void or_jumping(Node *self, int t, int f);

Or *or_new(Token *tok, Expr *x1, Expr *x2)
{
    Or *o = malloc(sizeof(Or));

    o->base.base.op   = tok;
    o->base.base.type = Type_Bool;

    o->base.e1 = x1;
    o->base.e2 = x2;

    o->base.base.base.jumping = or_jumping;

    return o;
}

static void or_jumping(Node *self, int t, int f)
{
    Or *o = (Or *)self;

    int label = (t != 0) ? t : newlabel();

    o->base.e1->base.base.jumping((Node *)o->base.e1, label, 0);
    o->base.e2->base.base.jumping((Node *)o->base.e2, t, f);

    if (t == 0)
        emitlabel(label);
}




static void rel_jumping(Node *self, int t, int f);

Rel *rel_new(Token *tok, Expr *x1, Expr *x2)
{
    Rel *r = malloc(sizeof(Rel));

    r->base.base.op   = tok;
    r->base.base.type = Type_Bool;

    r->base.e1 = x1;
    r->base.e2 = x2;

    r->base.base.base.jumping = rel_jumping;

    return r;
}

static void rel_jumping(Node *self, int t, int f)
{
    Rel *r = (Rel *)self;

    Expr *a = (Expr *)r->base.e1->base.base.reduce((Node *)r->base.e1);
    Expr *b = (Expr *)r->base.e2->base.base.reduce((Node *)r->base.e2);

    char *sa = a->base.base.tostring((Node *)a);
    char *op = token_tostring(r->base.base.op);
    char *sb = b->base.base.tostring((Node *)b);

    size_t len = strlen(sa) + strlen(op) + strlen(sb) + 5;
    char *test = malloc(len);
    snprintf(test, len, "%s %s %s", sa, op, sb);

    emitJumps(test, t, f);
}


static void seq_gen(Node *self, int b, int a);

Seq *seq_new(Stmt *s1, Stmt *s2)
{
    Seq *s = malloc(sizeof(Seq));
    s->s1 = s1;
    s->s2 = s2;
    s->base.base.gen = seq_gen;
    return s;
}

static void seq_gen(Node *self, int b, int a)
{
    Seq *s = (Seq *)self;

    if (s->s1 == Stmt_Null)
        s->s2->base.gen((Node *)s->s2, b, a);
    else if (s->s2 == Stmt_Null)
        s->s1->base.gen((Node *)s->s1, b, a);
    else {
        int label = node_newlabel();
        s->s1->base.gen((Node *)s->s1, b, label);
        node_emitlabel(label);
        s->s2->base.gen((Node *)s->s2, label, a);
    }
}


static void set_gen(Node *self, int b, int a);

static Type *set_check(Type *p1, Type *p2)
{
    if (type_numeric(p1) && type_numeric(p2)) return p2;
    if (p1 == Type_Bool && p2 == Type_Bool) return p2;
    return NULL;
}

Set *set_new(Id *id, Expr *expr)
{
    Set *s = malloc(sizeof(Set));

    s->id   = id;
    s->expr = expr;

    if (set_check(id->base.type, expr->type) == NULL)
        node_error((Node *)s, "type error");

    s->base.base.gen = set_gen;

    return s;
}

static void set_gen(Node *self, int b, int a)
{
    Set *s = (Set *)self;

    Expr *g = (Expr *)s->expr->base.base.gen((Node *)s->expr);

    char *lhs = s->id->base.base.tostring((Node *)s->id);
    char *rhs = g->base.base.tostring((Node *)g);

    node_emit("%s = %s", lhs, rhs);
}



static void setelem_gen(Node *self, int b, int a);

static Type *setelem_check(Type *p1, Type *p2)
{
    if (type_is_array(p1) || type_is_array(p2)) return NULL;
    if (p1 == p2) return p2;
    if (type_numeric(p1) && type_numeric(p2)) return p2;
    return NULL;
}

SetElem *setelem_new(Access *x, Expr *y)
{
    SetElem *s = malloc(sizeof(SetElem));

    s->array = x->array;
    s->index = x->index;
    s->expr  = y;

    if (setelem_check(x->base.base.type, y->type) == NULL)
        node_error((Node *)s, "type error");

    s->base.base.gen = setelem_gen;

    return s;
}

static void setelem_gen(Node *self, int b, int a)
{
    SetElem *s = (SetElem *)self;

    Expr *i = (Expr *)s->index->base.base.reduce((Node *)s->index);
    Expr *v = (Expr *)s->expr->base.base.reduce((Node *)s->expr);

    char *si = i->base.base.tostring((Node *)i);
    char *sv = v->base.base.tostring((Node *)v);
    char *arr = s->array->base.base.tostring((Node *)s->array);

    node_emit("%s [ %s ] = %s", arr, si, sv);
}



Stmt *Stmt_Null = NULL;
Stmt *Stmt_Enclosing = NULL;

Stmt *stmt_new()
{
    Stmt *s = malloc(sizeof(Stmt));
    s->after = 0;
    s->base.gen = NULL;
    s->base.jumping = NULL;
    s->base.tostring = NULL;
    return s;
}

__attribute__((constructor))
static void init_stmt_singletons()
{
    Stmt_Null = stmt_new();
    Stmt_Enclosing = Stmt_Null;
}



static int temp_count = 0;

static char *temp_tostring(Node *self)
{
    Temp *t = (Temp *)self;
    char *buf = malloc(32);
    snprintf(buf, 32, "t%d", t->number);
    return buf;
}

Temp *temp_new(Type *type)
{
    Temp *t = malloc(sizeof(Temp));

    t->base.op   = Word_temp();
    t->base.type = type;

    t->number = temp_count++;

    t->base.base.tostring = temp_tostring;

    return t;
}



static Node *unary_gen(Node *self);
static char *unary_tostring(Node *self);

Unary *unary_new(Token *tok, Expr *expr)
{
    Unary *u = malloc(sizeof(Unary));

    u->base.base.op   = tok;
    u->base.base.type = type_max(Type_Int, expr->type);

    if (!u->base.base.type)
        node_error((Node *)u, "type error");

    u->expr = expr;

    u->base.base.base.gen      = unary_gen;
    u->base.base.base.tostring = unary_tostring;

    return u;
}

static Node *unary_gen(Node *self)
{
    Unary *u = (Unary *)self;

    Expr *r = (Expr *)u->expr->base.base.reduce((Node *)u->expr);

    return (Node *)unary_new(u->base.base.op, r);
}

static char *unary_tostring(Node *self)
{
    Unary *u = (Unary *)self;

    char *op = token_tostring(u->base.base.op);
    char *s  = u->expr->base.base.tostring((Node *)u->expr);

    size_t len = strlen(op) + strlen(s) + 5;
    char *buf = malloc(len);

    snprintf(buf, len, "%s %s", op, s);
    return buf;
}



static void while_gen(Node *self, int b, int a);

While *while_new()
{
    While *w = malloc(sizeof(While));
    w->expr = NULL;
    w->stmt = NULL;
    w->base.base.gen = while_gen;
    return w;
}

void while_init(While *w, Expr *expr, Stmt *stmt)
{
    w->expr = expr;
    w->stmt = stmt;

    if (expr->type != Type_Bool)
        node_error((Node *)w, "boolean required in a while");
}

static void while_gen(Node *self, int b, int a)
{
    While *w = (While *)self;

    w->base.after = a;

    w->expr->base.base.jumping((Node *)w->expr, 0, a);

    int label = node_newlabel();
    node_emitlabel(label);

    w->stmt->base.gen((Node *)w->stmt, label, b);

    node_emit("goto L%d", b);
}


