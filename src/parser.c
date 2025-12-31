#include "parser.h"

static void parser_move(Parser *p)
{
    p->look = lexer_scan(p->lex);
}

static void parser_error(Parser *p, const char *s)
{
    fprintf(stderr, "near line %d: %s\n", p->lex->line, s);
    exit(1);
}

static void parser_match(Parser *p, int t)
{
    if (p->look->tag == t)
        parser_move(p);
    else
        parser_error(p, "syntax error");
}

Parser *parser_new(Lexer *lex)
{
    Parser *p = malloc(sizeof(Parser));
    p->lex  = lex;
    p->top  = NULL;
    p->used = 0;
    parser_move(p);
    return p;
}

void parser_program(Parser *p)
{
    Stmt *s = parser_block(p);
    int begin = node_newlabel();
    int after = node_newlabel();
    node_emitlabel(begin);
    s->base.gen((Node *)s, begin, after);
    node_emitlabel(after);
}

Stmt *parser_block(Parser *p)
{
    parser_match(p, '{');
    Env *saved = p->top;
    p->top = env_new(p->top);
    parser_decls(p);
    Stmt *s = parser_stmts(p);
    parser_match(p, '}');
    p->top = saved;
    return s;
}

void parser_decls(Parser *p)
{
    while (p->look->tag == TAG_BASIC) {
        Type *tp = parser_type(p);
        Token *tok = p->look;
        parser_match(p, TAG_ID);
        parser_match(p, ';');
        Id *id = id_new((Word *)tok, tp, p->used);
        env_put(p->top, tok, id);
        p->used += tp->width;
    }
}

Type *parser_type(Parser *p)
{
    Type *tp = (Type *)p->look;
    parser_match(p, TAG_BASIC);
    if (p->look->tag != '[')
        return tp;
    return parser_dims(p, tp);
}

Type *parser_dims(Parser *p, Type *tp)
{
    parser_match(p, '[');
    Token *tok = p->look;
    parser_match(p, TAG_NUM);
    parser_match(p, ']');
    if (p->look->tag == '[')
        tp = parser_dims(p, tp);
    return (Type *)array_new(((Num *)tok)->value, tp);
}

Stmt *parser_stmts(Parser *p)
{
    if (p->look->tag == '}')
        return Stmt_Null;
    return (Stmt *)seq_new(parser_stmt(p), parser_stmts(p));
}

Stmt *parser_stmt(Parser *p)
{
    Expr *x;
    Stmt *s, *s1, *s2;
    Stmt *saved;

    switch (p->look->tag) {
    case ';':
        parser_move(p);
        return Stmt_Null;
    case TAG_IF:
        parser_match(p, TAG_IF);
        parser_match(p, '(');
        x = parser_bool(p);
        parser_match(p, ')');
        s1 = parser_stmt(p);
        if (p->look->tag != TAG_ELSE)
            return (Stmt *)if_new(x, s1);
        parser_match(p, TAG_ELSE);
        s2 = parser_stmt(p);
        return (Stmt *)else_new(x, s1, s2);
    case TAG_WHILE: {
        While *wn = while_new();
        saved = Stmt_Enclosing;
        Stmt_Enclosing = (Stmt *)wn;
        parser_match(p, TAG_WHILE);
        parser_match(p, '(');
        x = parser_bool(p);
        parser_match(p, ')');
        s1 = parser_stmt(p);
        while_init(wn, x, s1);
        Stmt_Enclosing = saved;
        return (Stmt *)wn;
    }
    case TAG_DO: {
        Do *dn = do_new();
        saved = Stmt_Enclosing;
        Stmt_Enclosing = (Stmt *)dn;
        parser_match(p, TAG_DO);
        s1 = parser_stmt(p);
        parser_match(p, TAG_WHILE);
        parser_match(p, '(');
        x = parser_bool(p);
        parser_match(p, ')');
        parser_match(p, ';');
        do_init(dn, s1, x);
        Stmt_Enclosing = saved;
        return (Stmt *)dn;
    }
    case TAG_BREAK:
        parser_match(p, TAG_BREAK);
        parser_match(p, ';');
        return (Stmt *)break_new();
    case '{':
        return parser_block(p);
    default:
        return parser_assign(p);
    }
}

Stmt *parser_assign(Parser *p)
{
    Stmt *s;
    Token *t = p->look;
    parser_match(p, TAG_ID);
    Id *id = env_get(p->top, t);
    if (!id)
        parser_error(p, token_tostring(t));
    if (p->look->tag == '=') {
        parser_move(p);
        s = (Stmt *)set_new(id, parser_bool(p));
    } else {
        Access *x = parser_offset(p, id);
        parser_match(p, '=');
        s = (Stmt *)setelem_new(x, parser_bool(p));
    }
    parser_match(p, ';');
    return s;
}

Expr *parser_bool(Parser *p)
{
    Expr *x = parser_join(p);
    while (p->look->tag == TAG_OR) {
        Token *tok = p->look;
        parser_move(p);
        x = (Expr *)or_new(tok, x, parser_join(p));
    }
    return x;
}

Expr *parser_join(Parser *p)
{
    Expr *x = parser_rel(p);
    while (p->look->tag == TAG_EQ || p->look->tag == TAG_NEQ) {
        Token *tok = p->look;
        parser_move(p);
        x = (Expr *)rel_new(tok, x, parser_rel(p));
    }
    return x;
}

Expr *parser_rel(Parser *p)
{
    Expr *x = parser_expr(p);
    switch (p->look->tag) {
    case '<':
    case TAG_LEQ:
    case TAG_GEQ:
    case '>': {
        Token *tok = p->look;
        parser_move(p);
        return (Expr *)rel_new(tok, x, parser_expr(p));
    }
    default:
        return x;
    }
}

Expr *parser_expr(Parser *p)
{
    Expr *x = parser_term(p);
    while (p->look->tag == '+' || p->look->tag == '-') {
        Token *tok = p->look;
        parser_move(p);
        x = (Expr *)arith_new(tok, x, parser_term(p));
    }
    return x;
}

Expr *parser_term(Parser *p)
{
    Expr *x = parser_unary(p);
    while (p->look->tag == '*' || p->look->tag == '/') {
        Token *tok = p->look;
        parser_move(p);
        x = (Expr *)arith_new(tok, x, parser_unary(p));
    }
    return x;
}

Expr *parser_unary(Parser *p)
{
    if (p->look->tag == '-') {
        parser_move(p);
        return (Expr *)unary_new(Word_minus(), parser_unary(p));
    } else if (p->look->tag == '!') {
        Token *tok = p->look;
        parser_move(p);
        return (Expr *)not_new(tok, parser_unary(p));
    }
    return parser_factor(p);
}

Expr *parser_factor(Parser *p)
{
    Expr *x = NULL;
    switch (p->look->tag) {
    case '(':
        parser_move(p);
        x = parser_bool(p);
        parser_match(p, ')');
        return x;
    case TAG_NUM:
        x = (Expr *)constant_new(p->look, Type_Int);
        parser_move(p);
        return x;
    case TAG_REAL:
        x = (Expr *)constant_new(p->look, Type_Float);
        parser_move(p);
        return x;
    case TAG_TRUE:
        x = (Expr *)Constant_true;
        parser_move(p);
        return x;
    case TAG_FALSE:
        x = (Expr *)Constant_false;
        parser_move(p);
        return x;
    case TAG_ID: {
        Id *id = env_get(p->top, p->look);
        if (!id)
            parser_error(p, "undeclared id");
        parser_move(p);
        if (p->look->tag != '[')
            return (Expr *)id;
        return (Expr *)parser_offset(p, id);
    }
    default:
        parser_error(p, "syntax error");
        return NULL;
    }
}

Access *parser_offset(Parser *p, Id *a)
{
    Expr *i;
    Expr *w;
    Expr *t1, *t2;
    Expr *loc;
    Type *type = a->base.type;

    parser_match(p, '[');
    i = parser_bool(p);
    parser_match(p, ']');
    type = ((Array *)type)->type;
    w = (Expr *)constant_new_num(type->width);
    t1 = (Expr *)arith_new(token_new('*'), i, w);
    loc = t1;

    while (p->look->tag == '[') {
        parser_match(p, '[');
        i = parser_bool(p);
        parser_match(p, ']');
        type = ((Array *)type)->type;
        w = (Expr *)constant_new_num(type->width);
        t1 = (Expr *)arith_new(token_new('*'), i, w);
        t2 = (Expr *)arith_new(token_new('+'), loc, t1);
        loc = t2;
    }

    return access_new((Expr *)a, loc, type);
}

