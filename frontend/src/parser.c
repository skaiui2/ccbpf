#include "parser.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern struct Type     *Type_Int;
extern struct Type     *Type_Float;
extern struct Type     *Type_Bool;
extern struct Constant *Constant_true;
extern struct Constant *Constant_false;

static struct Type *basic_type_from_token(struct lexer_token *tok)
{
    if (!tok || !tok->lexeme) return NULL;

    if (strcmp(tok->lexeme, "int") == 0)   return Type_Int;
    if (strcmp(tok->lexeme, "float") == 0) return Type_Float;
    if (strcmp(tok->lexeme, "bool") == 0)  return Type_Bool;

    return NULL;
}


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

static void parser_move(struct Parser *p)
{
    p->look = lexer_scan(p->lex);
    if (p->look) { 
        char *s = token_to_string(p->look); 
        printf("TOKEN: tag=%d, str=%s\n", p->look->tag, s); 
        free(s); 
    }
}

static void parser_error(struct Parser *p, const char *msg)
{
    fprintf(stderr, "near line %d: %s \n", p->lex->line, msg);
    exit(1);
}

static void parser_match(struct Parser *p, int tag)
{
    if (p->look->tag == tag)
        parser_move(p);
    else
        parser_error(p, "syntax error");
}

struct Parser *parser_new(struct lexer *lex)
{
    struct Parser *p = malloc(sizeof(struct Parser));
    p->lex  = lex;
    p->top  = NULL;
    p->used = 0;
    parser_move(p);
    return p;
}

void parser_program(struct Parser *p)
{
    struct Stmt *s = parser_block(p);
    int begin = node_newlabel();
    int after = node_newlabel();
    node_emitlabel(begin);
    s->base.gen((struct Node *)s, begin, after);
    node_emitlabel(after);
}

struct Stmt *parser_block(struct Parser *p)
{
    parser_match(p, LBRACE);
    struct Env *saved = p->top;
    p->top = env_new(p->top);
    parser_decls(p);
    struct Stmt *s = parser_stmts(p);
    parser_match(p, RBRACE);
    p->top = saved;
    return s;
}

void parser_decls(struct Parser *p)
{
    while (p->look->tag == BASIC) {
        struct Type *tp = parser_type(p);    
        struct lexer_token *tok = p->look;   
        parser_match(p, ID);

        if (p->look->tag == LBRACKET) {     
            parser_move(p);

            if (p->look->tag != NUM)
                parser_error(p, "array size must be a number");

            int size = p->look->int_val;
            parser_move(p);
            parser_match(p, RBRACKET);     

            tp = (struct Type *)array_new(tp, size);
        }

        parser_match(p, SEMICOLON);

        if (!tok->lexeme)
            parser_error(p, "identifier without lexeme");

        struct Id *id = id_new(tok, tp, p->used);
        env_put_var(p->top, tok->lexeme, id);
        p->used += tp->width;
    }
}

struct Type *parser_type(struct Parser *p)
{
    if (p->look->tag != BASIC)
        parser_error(p, "type expected");

    struct Type *tp = basic_type_from_token(p->look);
    if (!tp)
        parser_error(p, "unknown basic type");

    parser_match(p, BASIC);

    if (p->look->tag != LBRACKET)
        return tp;

    return parser_dims(p, tp);
}

struct Type *parser_dims(struct Parser *p, struct Type *base)
{
    parser_match(p, LBRACKET);
    struct lexer_token *tok = p->look;
    parser_match(p, NUM);
    parser_match(p, RBRACKET);

    int size = tok->int_val;

    if (p->look->tag == LBRACKET)
        base = parser_dims(p, base);

    struct Array *arr = array_new(base, size);
    return (struct Type *)arr;
}

struct Stmt *parser_stmts(struct Parser *p)
{
    if (p->look->tag == RBRACE)
        return Stmt_Null;
    struct Stmt *this = parser_stmt(p);
    struct Stmt *other = parser_stmts(p);
    return (struct Stmt *)seq_new(this, other);
}

struct Stmt *parser_stmt(struct Parser *p)
{
    struct Expr *x;
    struct Stmt *s1, *s2;
    struct Stmt *saved;

    switch (p->look->tag) {
    case SEMICOLON:
        parser_move(p);
        return Stmt_Null;

    case IF:
        parser_match(p, IF);
        parser_match(p, LPAREN);
        x = parser_bool(p);
        parser_match(p, RPAREN);
        s1 = parser_stmt(p);
        if (p->look->tag != ELSE)
            return (struct Stmt *)if_new(x, s1);
        parser_match(p, ELSE);
        s2 = parser_stmt(p);
        return (struct Stmt *)else_new(x, s1, s2);

    case WHILE: {
        struct While *wn = while_new();
        saved = Stmt_Enclosing;
        Stmt_Enclosing = (struct Stmt *)wn;
        parser_match(p, WHILE);
        parser_match(p, LPAREN);
        x = parser_bool(p);
        parser_match(p, RPAREN);
        s1 = parser_stmt(p);
        while_init(wn, x, s1);
        Stmt_Enclosing = saved;
        return (struct Stmt *)wn;
    }

    case DO: {
        struct Do *dn = do_new();
        saved = Stmt_Enclosing;
        Stmt_Enclosing = (struct Stmt *)dn;
        parser_match(p, DO);
        s1 = parser_stmt(p);
        parser_match(p, WHILE);
        parser_match(p, LPAREN);
        x = parser_bool(p);
        parser_match(p, RPAREN);
        parser_match(p, SEMICOLON);
        do_init(dn, s1, x);
        Stmt_Enclosing = saved;
        return (struct Stmt *)dn;
    }

    case BREAK:
        parser_match(p, BREAK);
        parser_match(p, SEMICOLON);
        return (struct Stmt *)break_new();

    case LBRACE:
        return parser_block(p);

    default:
        return parser_assign(p);
    }
}

struct Stmt *parser_assign(struct Parser *p)
{
    struct Stmt *s;
    struct lexer_token *t = p->look;
    parser_match(p, ID);

    if (!t->lexeme)
        parser_error(p, "identifier without lexeme");

    struct Id *id = env_get_var(p->top, t->lexeme);
    if (!id)
        parser_error(p, "undeclared id");

    if (p->look->tag == ASSIGN) {
        parser_move(p);
        s = (struct Stmt *)set_new(id, parser_bool(p));
    } else {
        struct Access *x = parser_offset(p, id);
        parser_match(p, ASSIGN);
        s = (struct Stmt *)setelem_new(x, parser_bool(p));
    }
    parser_match(p, SEMICOLON);
    return s;
}

struct Expr *parser_bool(struct Parser *p)
{
    struct Expr *x = parser_join(p);
    while (p->look->tag == OR) {
        struct lexer_token *tok = p->look;
        parser_move(p);
        x = (struct Expr *)or_new(tok, x, parser_join(p));
    }
    return x;
}

struct Expr *parser_bitor(struct Parser *p)
{
    struct Expr *x = parser_rel(p);

    while (p->look->tag == OR_BIT) {  
        struct lexer_token *tok = p->look;
        parser_move(p);
        x = (struct Expr *)bitor_new(tok, x, parser_rel(p));
    }

    return x;
}

struct Expr *parser_bitand(struct Parser *p)
{
    struct Expr *x = parser_bitor(p);

    while (p->look->tag == AND_BIT) {   
        struct lexer_token *tok = p->look;
        parser_move(p);
        x = (struct Expr *)bitand_new(tok, x, parser_rel(p));
    }

    return x;
}

struct Expr *parser_join(struct Parser *p)
{
    struct Expr *x = parser_bitand(p);
    while (p->look->tag == AND) {
        struct lexer_token *tok = p->look;
        parser_move(p);
        x = (struct Expr *)and_new(tok, x, parser_rel(p));
    }
    return x;
}

struct Expr *parser_rel(struct Parser *p)
{
    struct Expr *x = parser_expr(p);

    switch (p->look->tag) {
    case LT:
    case GT:
    case LE:
    case GE:
    case EQ:
    case NE: {
        struct lexer_token *tok = p->look;
        parser_move(p);
        return (struct Expr *)rel_new(tok, x, parser_expr(p));
    }
    default:
        return x;
    }
}

struct Expr *parser_expr(struct Parser *p)
{
    struct Expr *x = parser_term(p);
    while (p->look->tag == PLUS || p->look->tag == MINUS) {
        struct lexer_token *tok = p->look;
        parser_move(p);
        x = (struct Expr *)arith_new(tok, x, parser_term(p));
    }
    return x;
}

struct Expr *parser_term(struct Parser *p)
{
    struct Expr *x = parser_unary(p);
    while (p->look->tag == STAR || p->look->tag == SLASH || p->look->tag == MOD) {
        struct lexer_token *tok = p->look;
        parser_move(p);
        x = (struct Expr *)arith_new(tok, x, parser_unary(p));
    }
    return x;
}

struct Expr *parser_unary(struct Parser *p)
{
    if (p->look->tag == MINUS) {
        struct lexer_token *tok = p->look;
        parser_move(p);
        return (struct Expr *)unary_new(tok, parser_unary(p));
    } else if (p->look->tag == NOT) {
        struct lexer_token *tok = p->look;
        parser_move(p);
        return (struct Expr *)not_new(tok, parser_unary(p));
    }
    return parser_factor(p);
}

struct Expr *parser_factor(struct Parser *p)
{
    struct Expr *x = NULL;

    switch (p->look->tag) {
    case LPAREN:
        parser_move(p);
        x = parser_bool(p);
        parser_match(p, RPAREN);
        return x;

    case NUM:
        x = (struct Expr *)constant_int(p->look->int_val);
        parser_move(p);
        return x;

    case REAL:
        x = (struct Expr *)constant_float(p->look->real_val);
        parser_move(p);
        return x;

    case TRUE:
        x = (struct Expr *)Constant_true;
        parser_move(p);
        return x;

    case FALSE:
        x = (struct Expr *)Constant_false;
        parser_move(p);
        return x;

    case ID: {
        if (!p->look->lexeme)
            parser_error(p, "identifier without lexeme");
        struct Id *id = env_get_var(p->top, p->look->lexeme);
        if (!id)
            parser_error(p, "undeclared id");
        parser_move(p);
        if (p->look->tag != LBRACKET)
            return (struct Expr *)id;
        return (struct Expr *)parser_offset(p, id);
    }

    default:
        parser_error(p, "syntax error");
        return NULL;
    }
}

static struct lexer_token TOK_MUL  = { STAR,  0, .lexeme = "*" };
static struct lexer_token TOK_PLUS = { PLUS,  0, .lexeme = "+" };

struct Access *parser_offset(struct Parser *p, struct Id *a)
{
    struct Expr *i;
    struct Type *type = a->base.type;

    parser_match(p, LBRACKET);
    i = parser_bool(p);          /* 这里的i就是元素索引 */
    parser_match(p, RBRACKET);

    struct Array *arr = (struct Array *)type;
    type = arr->of;

    /* 只支持一维数组：直接把i当作index传下去 */
    return access_new((struct Expr *)a, i, type);
}
