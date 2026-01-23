#include "parser.h"
#include "bpf_types.h"
#include "inter.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern struct Type     *Type_Int;
extern struct Type     *Type_Float;
extern struct Type     *Type_Bool;
extern struct Type     *Type_Byte;
extern struct Type     *Type_Short;
extern struct Constant *Constant_true;
extern struct Constant *Constant_false;

static struct Type *basic_type_from_token(struct lexer_token *tok)
{
    if (!tok || !tok->lexeme) return NULL;

    if (strcmp(tok->lexeme, "int") == 0)   return Type_Int;
    if (strcmp(tok->lexeme, "float") == 0) return Type_Float;
    if (strcmp(tok->lexeme, "bool") == 0)  return Type_Bool;
    if (strcmp(tok->lexeme, "char") == 0)  return Type_Byte;
    if (strcmp(tok->lexeme, "short") == 0)  return Type_Short;

    return NULL;
}


char *token_to_string(struct lexer_token *tok)
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
    case ARROW:      return strdup("->");
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
    p->top = env_new(NULL);
    p->used = 0;
    parser_move(p);
    return p;
}


static void parser_struct_decl(struct Parser *p)
{
    parser_match(p, STRUCT);

    if (p->look->tag != ID || !p->look->lexeme)
        parser_error(p, "expected struct name");

    char *name = strdup(p->look->lexeme);
    parser_match(p, ID);

    struct StructType *st = struct_new();
    int offset = 0;

    parser_match(p, LBRACE);

    while (p->look->tag == BASIC) {
        struct Type *ft = parser_type(p);

        if (p->look->tag != ID || !p->look->lexeme)
            parser_error(p, "expected field name");

        char *fname = strdup(p->look->lexeme);
        parser_match(p, ID);
        parser_match(p, SEMICOLON);

        struct StructFieldInfo *fi = malloc(sizeof(*fi));
        fi->offset = offset;
        fi->type   = ft;

        hashmap_put(&st->fields, fname, fi);

        offset += ft->width;
    }

    st->base.width = offset;

    parser_match(p, RBRACE);
    parser_match(p, SEMICOLON);

    env_put_type(p->top, name, (struct Type *)st);
}

struct Access *parser_field(struct Parser *p, struct Expr *base, struct lexer_token *field_tok)
{
    /* id-based struct pointer */
    if (base->base.tag == TAG_ID) {
        struct Id *id = (struct Id *)base;

        if (id->base_offset >= 0 && id->st != NULL) {
            struct StructFieldInfo *fi =
                hashmap_get(&id->st->fields, field_tok->lexeme);
            if (!fi)
                parser_error(p, "unknown field in struct");

            int final_offset = id->base_offset + fi->offset;

            if (id->is_ctx_ptr) {
                struct Expr *ctx_e = ctx_load_expr_new(final_offset);
                ctx_e->type = fi->type;
                return (struct Access *)ctx_e;
            }

            parser_error(p, "struct pointer missing ctx tag");
        }
    }

    /* ctx_ptr->field */
    if (base->base.tag == TAG_CTX_PTR) {
        struct CtxPtrExpr *pp = (struct CtxPtrExpr *)base;

        if (!pp->st)
            parser_error(p, "ctx pointer missing struct type");

        struct StructFieldInfo *fi =
            hashmap_get(&pp->st->fields, field_tok->lexeme);
        if (!fi)
            parser_error(p, "unknown field in struct");

        int final_offset = pp->base_offset + fi->offset;

        struct Expr *ctx_e = ctx_load_expr_new(final_offset);
        ctx_e->type = fi->type;
        return (struct Access *)ctx_e;
    }

    parser_error(p, "base of '->' must be ctx struct pointer");
    return NULL;
}

static struct Stmt *parse_hook_function(struct Parser *p)
{
    if (p->look->tag != BASIC || strcmp(p->look->lexeme, "int") != 0)
        parser_error(p, "program must start with int hook(...)");
    parser_move(p);

    if (p->look->tag != ID || strcmp(p->look->lexeme, "hook") != 0)
        parser_error(p, "expected hook");
    parser_move(p);

    parser_match(p, LPAREN);

    if (p->look->tag != ID || strcmp(p->look->lexeme, "void") != 0)
        parser_error(p, "expected void");
    parser_move(p);

    parser_match(p, STAR);

    if (p->look->tag != ID || strcmp(p->look->lexeme, "ctx") != 0)
        parser_error(p, "expected ctx");
    parser_move(p);

    parser_match(p, RPAREN);

    struct Type *void_base = Type_Int;
    struct PtrType *void_ptr = ptr_new(void_base);
    struct Type *param_type = (struct Type *)void_ptr;

    {
        struct Id *id_ctx = id_new_from_name("ctx", param_type, p->used);
        env_put_var(p->top, "ctx", id_ctx);
        p->used += param_type->width;
    }

    return parser_block(p);
}

void parser_program(struct Parser *p)
{
    while (p->look->tag == STRUCT) {
        parser_struct_decl(p);
    }

    struct Stmt *s = parse_hook_function(p);

    fprintf(stderr, "[PP] root stmt=%p tag=%d\n",
            (void *)s,
            s ? s->base.tag : -1);

    if (s == Stmt_Null) {
        fprintf(stderr, "[PP] root is Stmt_Null, nothing to generate\n");
    }

    int begin = node_newlabel();
    int after = node_newlabel();

    node_emitlabel(begin);
    if (s && s->base.gen) {
        s->base.gen((struct Node *)s, begin, after);
    } else {
        fprintf(stderr, "[PP] root stmt has no gen, tag=%d\n",
                s ? s->base.tag : -1);
    }
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

    fprintf(stderr, "[PP] block root stmt=%p tag=%d\n", (void *)s, s ? s->base.tag : -1);
    return s;
}


void parser_decls(struct Parser *p)
{
    while (p->look->tag == BASIC || p->look->tag == STRUCT) {
        if (p->look->tag == BASIC) {
            /* 原有 BASIC 声明逻辑 */
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
        else if (p->look->tag == STRUCT) {
            /* 新增：struct NAME *id; 形式 */
            parser_match(p, STRUCT);

            if (p->look->tag != ID || !p->look->lexeme)
                parser_error(p, "expected struct name");

            char *sname = strdup(p->look->lexeme);
            parser_match(p, ID);

            struct Type *st = env_get_type(p->top, sname);
            if (!st || st->tag != TYPE_STRUCT)
                parser_error(p, "unknown struct type in declaration");

            /* 目前只支持指针：struct NAME *id; */
            if (p->look->tag != STAR)
                parser_error(p, "only 'struct T *var;' is supported for now");

            parser_match(p, STAR);

            struct lexer_token *tok = p->look;
            parser_match(p, ID);

            parser_match(p, SEMICOLON);

            if (!tok->lexeme)
                parser_error(p, "identifier without lexeme");

            struct PtrType *pt = ptr_new(st);
            struct Id *id = id_new(tok, (struct Type *)pt, p->used);
            env_put_var(p->top, tok->lexeme, id);

            /* 指针本身占用一个槽位（按指针宽度） */
            p->used += pt->base.width;
        }
        else {
            break;
        }
    }
}

struct Type *parser_type(struct Parser *p)
{
    /* ===== unsigned, only：
     *   unsigned short
     *   unsigned int
     *   unsigned char
     * errors: unsigned short int / unsigned int int / unsigned char int
     */
    if (p->look->tag == BASIC &&
        p->look->lexeme &&
        strcmp(p->look->lexeme, "unsigned") == 0)
    {
        parser_match(p, BASIC);  // eat "unsigned"

        if (p->look->tag != BASIC || !p->look->lexeme)
            parser_error(p, "expected type after 'unsigned'");

        struct Type *tp = NULL;

        if (strcmp(p->look->lexeme, "short") == 0) {
            tp = Type_Short;
        } else if (strcmp(p->look->lexeme, "int") == 0) {
            tp = Type_Int;
        } else if (strcmp(p->look->lexeme, "char") == 0) {
            tp = Type_Byte;
        } else {
            parser_error(p, "only 'unsigned short', 'unsigned int', 'unsigned char' are supported");
        }

        parser_match(p, BASIC);  // eat short/int/char

        if (p->look->tag == BASIC &&
            p->look->lexeme &&
            strcmp(p->look->lexeme, "int") == 0)
        {
            parser_error(p, "unexpected 'int' after 'unsigned' type");
        }

        /* ptr */
        while (p->look->tag == STAR) {
            parser_match(p, STAR);
            tp = (struct Type *)ptr_new(tp);
        }

        return tp;
    }

    if (p->look->tag == BASIC) {
        struct Type *tp = basic_type_from_token(p->look);
        if (!tp)
            parser_error(p, "unknown basic type");

        parser_match(p, BASIC);

        while (p->look->tag == STAR) {
            parser_match(p, STAR);
            tp = (struct Type *)ptr_new(tp);
        }

        return tp;
    }

    if (p->look->tag == STRUCT) {
        parser_match(p, STRUCT);

        if (p->look->tag != ID)
            parser_error(p, "expected struct name");

        struct Type *tp = env_get_type(p->top, p->look->lexeme);
        if (!tp || tp->tag != TYPE_STRUCT)
            parser_error(p, "unknown struct type");

        parser_match(p, ID);

        /* ptr */
        while (p->look->tag == STAR) {
            parser_match(p, STAR);
            tp = (struct Type *)ptr_new(tp);
        }

        return tp;
    }

    return NULL; 
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
    if (!p->look || p->look->tag == RBRACE || p->look->tag == 0)
        return Stmt_Null;

    struct Stmt *this  = parser_stmt(p);
    struct Stmt *other = parser_stmts(p);

    return (struct Stmt *)seq_new(this, other);
}


struct Stmt *parser_stmt(struct Parser *p)
{
    struct Expr *x;
    struct Stmt *s1, *s2;
    struct Stmt *saved;
    if (!p->look || p->look->tag == 0 || p->look->tag == RBRACE) { 
        return Stmt_Null;
    }

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

    case RETURN:
        parser_match(p, RETURN);
        x = parser_bool(p);
        parser_match(p, SEMICOLON);
        return (struct Stmt *)return_new(x);

    default: {
        /* 如果是内建函数开头：ntohl/ntohs/print，直接当表达式语句处理 */
        if (p->look->tag == ID && p->look->lexeme) {
            const char *name = p->look->lexeme;

            if (strcmp(name, "ntohl") == 0 ||
                strcmp(name, "ntohs") == 0 ||
                strcmp(name, "print") == 0) {

                struct Expr *e = parser_bool(p);
                parser_match(p, SEMICOLON);
                return (struct Stmt *)e;   /* 你的 Stmt/Expr 体系本来就混用 Node* */
            }
        }

        /* 其他情况，按原来的逻辑，当成赋值语句 */
        return parser_assign(p);
    }

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
        struct Expr *rhs = parser_bool(p);

        if (rhs->base.tag == TAG_CTX_PTR) {
            struct CtxPtrExpr *pp = (struct CtxPtrExpr *)rhs;
            id->is_ctx_ptr  = 1;
            id->st          = pp->st;
            id->base_offset = pp->base_offset;
        }

        s = (struct Stmt *)set_new(id, rhs);
        goto done;
    }

    {
        struct Access *x = parser_offset(p, id);
        if (p->look->tag != ASSIGN)
            parser_error(p, "expected '='");
        parser_move(p);
        s = (struct Stmt *)setelem_new(x, parser_bool(p));
    }

done:
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
    case LE:
    case GT:
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

struct Expr *parser_postfix(struct Parser *p)
{
    struct Expr *e = parser_factor(p);

    for (;;) {

        if (p->look->tag == LBRACKET) {
            if (e->base.tag != TAG_ID)
                parser_error(p, "subscript requires identifier");

            e = (struct Expr *)parser_offset(p, (struct Id *)e);
            continue;
        }

        if (p->look->tag == ARROW) {
            parser_match(p, ARROW);

            if (p->look->tag != ID || !p->look->lexeme)
                parser_error(p, "expected field name after '->'");

            struct lexer_token *field_tok = p->look;
            parser_match(p, ID);

            e = (struct Expr *)parser_field(p, e, field_tok);
            continue;
        }

        break;
    }

    return e;
}

struct Expr *parser_unary(struct Parser *p)
{
    if (p->look->tag == AND_BIT) {
        parser_move(p);

        struct Expr *e = parser_unary(p);

        if (e->base.tag == TAG_CTX) {
            struct CtxExpr *c = (struct CtxExpr *)e;
            return (struct Expr *)ctx_ptr_new(c->offset, c->base.type);
        }

        parser_error(p, "unsupported &expr (only &ctx[const] allowed)");
    }

    if (p->look->tag == MINUS) {
        struct lexer_token *tok = p->look;
        parser_move(p);
        return (struct Expr *)unary_new(tok, parser_unary(p));
    }

    if (p->look->tag == NOT) {
        struct lexer_token *tok = p->look;
        parser_move(p);
        return (struct Expr *)not_new(tok, parser_unary(p));
    }

    return parser_postfix(p);
}

struct Expr *parser_factor(struct Parser *p)
{
    struct Expr *x = NULL;

    switch (p->look->tag) {

    case LPAREN: {
        parser_move(p);

        struct Type *ty = parser_type(p);
        if (ty != NULL) {
            parser_match(p, RPAREN);
            struct Expr *e = parser_unary(p);

            /* ===== (struct T *)&ctx[k] ===== */
            if (e->base.tag == TAG_CTX_PTR) {
                struct CtxPtrExpr *pp = (struct CtxPtrExpr *)e;

                if (ty->tag == TYPE_PTR) {
                    struct PtrType *pt = (struct PtrType *)ty;

                    if (pt->to->tag == TYPE_STRUCT) {
                        pp->st        = (struct StructType *)pt->to;
                        pp->base.type = ty;
                        return e;
                    }
                }
                parser_error(p, "unsupported cast on ctx pointer");
            }

            /* ===== (struct T *)ctx ===== */
            if (e->base.tag == TAG_ID) {
                struct Id *id = (struct Id *)e;

                if (strcmp(id->base.op->lexeme, "ctx") == 0) {
                    if (ty->tag == TYPE_PTR) {
                        struct PtrType *pt = (struct PtrType *)ty;

                        if (pt->to->tag == TYPE_STRUCT) {
                            struct CtxPtrExpr *pp = ctx_ptr_new(0, ty);
                            pp->st = (struct StructType *)pt->to;
                            return (struct Expr *)pp;
                        }
                    }
                    parser_error(p, "unsupported cast on ctx");
                }
            }

            /* ===== normal cast ===== */
            e->type = ty;
            return e;
        }

        /* grouped expression */
        x = parser_bool(p);
        parser_match(p, RPAREN);
        return x;
    }

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

        if (strcmp(p->look->lexeme, "ntohl") == 0) {
            parser_move(p);
            parser_match(p, LPAREN);
            struct Expr *arg = parser_bool(p);
            parser_match(p, RPAREN);
            return (struct Expr *)builtin_call_new(NATIVE_NTOHL, arg);
        }

        if (strcmp(p->look->lexeme, "ntohs") == 0) {
            parser_move(p);
            parser_match(p, LPAREN);
            struct Expr *arg = parser_bool(p);
            parser_match(p, RPAREN);
            return (struct Expr *)builtin_call_new(NATIVE_NTOHS, arg);
        }

        if (strcmp(p->look->lexeme, "print") == 0) {
            parser_move(p);
            parser_match(p, LPAREN);
            struct Expr *arg = parser_bool(p);
            parser_match(p, RPAREN);
            return (struct Expr *)builtin_call_new(NATIVE_PRINTF, arg);
        }

        struct Id *id = env_get_var(p->top, p->look->lexeme);
        if (!id)
            parser_error(p, "undeclared id");

        parser_move(p);
        return (struct Expr *)id;
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
    parser_match(p, LBRACKET);
    struct Expr *idx = parser_bool(p);
    parser_match(p, RBRACKET);

    if (a->is_ctx_ptr) {
        if (idx->base.tag != TAG_CONSTANT)
            parser_error(p, "ctx[] index must be constant");

        int off = ((struct Constant *)idx)->int_val;
        return (struct Access *)ctx_load_expr_new(off);
    }

    struct Type *type = a->base.type;
    if (type->tag != TYPE_ARRAY)
        parser_error(p, "subscript on non-array");

    struct Array *arr = (struct Array *)type;
    return access_new((struct Expr *)a, idx, arr->of);
}
