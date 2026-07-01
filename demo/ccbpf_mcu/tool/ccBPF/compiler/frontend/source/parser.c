#include "parser.h"
#include "inter.h"
#include "mg_alloc.h"
#include "hashmap.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define COMPILER_DEBUG_ENABLED 0

static void compiler_debug(const char *fmt, ...)
{
#if COMPILER_DEBUG_ENABLED
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
#else
    (void)fmt;
#endif
}

extern struct Type     *Type_Int;
extern struct Type     *Type_Bool;
extern struct Type     *Type_Byte;
extern struct Type     *Type_Short;
extern struct Constant *Constant_true;
extern struct Constant *Constant_false;


struct NativeDecl {
    const char *name;
    int         id;     
    int         argc;
};

struct hashmap native_decl_table; 

void native_decl_register(const char *name, int id, int argc)
{
    struct NativeDecl *d = mg_region_alloc(longterm_region, sizeof(*d));
    d->name = region_strdup(name);
    d->id   = id;
    d->argc = argc;

    hashmap_put(&native_decl_table, (void*)d->name, d);
}

static struct Type *basic_type_from_token(struct lexer_token *tok)
{
    if (!tok || !tok->lexeme) return NULL;
    if (strcmp(tok->lexeme, "int") == 0)   return Type_Int;
    if (strcmp(tok->lexeme, "bool") == 0)  return Type_Bool;
    if (strcmp(tok->lexeme, "char") == 0)  return Type_Byte;
    if (strcmp(tok->lexeme, "short") == 0) return Type_Short;
    return NULL;
}

char *token_to_string(struct lexer_token *tok)
{
    if (!tok) return region_strdup("<?>");
    if (tok->lexeme) return region_strdup(tok->lexeme);

    if (tok->tag == NUM) {
        char *buf = mg_region_alloc(longterm_region, 32);
        snprintf(buf, 32, "%d", tok->int_val);
        return buf;
    }

    switch (tok->tag) {
        case AND_BIT:    return region_strdup("&");
        case OR_BIT:     return region_strdup("|");
        case LT:         return region_strdup("<");
        case GT:         return region_strdup(">");
        case PLUS:       return region_strdup("+");
        case MINUS:      return region_strdup("-");
        case STAR:       return region_strdup("*");
        case SLASH:      return region_strdup("/");
        case MOD:        return region_strdup("%");
        case ASSIGN:     return region_strdup("=");
        case ADD_ASSIGN: return region_strdup("+=");
        case SUB_ASSIGN: return region_strdup("-=");
        case MUL_ASSIGN: return region_strdup("*=");
        case DIV_ASSIGN: return region_strdup("/=");
        case ARROW:      return region_strdup("->");
        case INC:        return region_strdup("++");
        case DEC:        return region_strdup("--");
        case EQ:         return region_strdup("==");
        case NE:         return region_strdup("!=");
        case LE:         return region_strdup("<=");
        case GE:         return region_strdup(">=");
        case AND:        return region_strdup("&&");
        case OR:         return region_strdup("||");
        case LPAREN:     return region_strdup("(");
        case RPAREN:     return region_strdup(")");
        case LBRACE:     return region_strdup("{");
        case RBRACE:     return region_strdup("}");
        case LBRACKET:   return region_strdup("[");
        case RBRACKET:   return region_strdup("]");
        case COMMA:      return region_strdup(",");
        case SEMICOLON:  return region_strdup(";");
        case DOT:        return region_strdup(".");
        default: {
            char *buf = mg_region_alloc(longterm_region, 16);
            snprintf(buf, 16, "#%d", tok->tag);
            return buf;
        }
    }
}

static void parser_move(struct Parser *p)
{
    p->look = lexer_scan(p->lex);
#if COMPILER_DEBUG_ENABLED
    if (p->look) {
        char *s = token_to_string(p->look);
        compiler_debug("TOKEN: tag=%d, str=%s\n", p->look->tag, s);
    }
#endif
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
    hashmap_init(&native_decl_table, 16, HASHMAP_KEY_STRING);
    struct Parser *p = mg_region_alloc(longterm_region, sizeof(struct Parser));
    p->lex  = lex;
    p->top  = env_new(NULL);
    p->used = 0;
    parser_move(p);
    return p;
}

static void parser_struct_decl(struct Parser *p)
{
    parser_match(p, STRUCT);

    if (p->look->tag != ID || !p->look->lexeme)
        parser_error(p, "expected struct name");

    char *name = region_strdup(p->look->lexeme);
    parser_match(p, ID);

    struct StructType *st = struct_new();
    int offset = 0;

    parser_match(p, LBRACE);

    while (p->look->tag == BASIC) {
        struct Type *ft = parser_type(p);

        if (p->look->tag != ID || !p->look->lexeme)
            parser_error(p, "expected field name");

        char *fname = region_strdup(p->look->lexeme);
        parser_match(p, ID);
        parser_match(p, SEMICOLON);

        struct StructFieldInfo *fi =
                mg_region_alloc(longterm_region, sizeof(*fi));
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

struct Stmt *parser_block(struct Parser *p)
{
    parser_match(p, LBRACE);
    struct Env *saved = p->top;
    p->top = env_new(p->top);
    parser_decls(p);
    struct Stmt *s = parser_stmts(p);
    parser_match(p, RBRACE);
    p->top = saved;

    compiler_debug("[PP] block root stmt=%p tag=%d\n",
                   (void *)s,
                   s ? s->base.tag : -1);

    return s;
}

static void parser_block_gen(struct Parser *p, int begin, int after)
{
    parser_match(p, LBRACE);
    struct Env *saved = p->top;
    p->top = env_new(p->top);

    parser_decls(p);

    while (p->look && p->look->tag != RBRACE && p->look->tag != 0) {
        struct Stmt *s = parser_stmt(p);

        compiler_debug("[PP] block stmt=%p tag=%d\n",
                       (void *)s,
                       s ? s->base.tag : -1);

        if (s && s->base.gen) {
            s->base.gen((struct Node *)s, begin, after);
        }
        mg_region_reset(frontend_region);
    }

    parser_match(p, RBRACE);
    p->top = saved;
}

static void parse_hook_function_gen(struct Parser *p, int begin, int after)
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

    parser_block_gen(p, begin, after);
}

void parser_program(struct Parser *p)
{
    while (p->look->tag == STRUCT) {
        parser_struct_decl(p);
        mg_region_reset(frontend_region);
    }

    int begin = node_newlabel();
    int after = node_newlabel();

    node_emitlabel(begin);
    parse_hook_function_gen(p, begin, after);
    node_emitlabel(after);
}

void parser_decls(struct Parser *p)
{
    while (p->look->tag == BASIC || p->look->tag == STRUCT) {
        if (p->look->tag == BASIC) {
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
            parser_match(p, STRUCT);

            if (p->look->tag != ID || !p->look->lexeme)
                parser_error(p, "expected struct name");

            char *sname = region_strdup(p->look->lexeme);
            parser_match(p, ID);

            struct Type *st = env_get_type(p->top, sname);
            if (!st || st->tag != TYPE_STRUCT)
                parser_error(p, "unknown struct type in declaration");

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

            p->used += pt->base.width;
        }
        else {
            break;
        }
    }
}

struct Type *parser_type(struct Parser *p)
{
    if (p->look->tag == BASIC &&
        p->look->lexeme &&
        strcmp(p->look->lexeme, "unsigned") == 0)
    {
        parser_match(p, BASIC);

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

        parser_match(p, BASIC);

        if (p->look->tag == BASIC &&
            p->look->lexeme &&
            strcmp(p->look->lexeme, "int") == 0)
        {
            parser_error(p, "unexpected 'int' after 'unsigned' type");
        }

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
    struct Stmt *result = Stmt_Null;

    while (p->look && p->look->tag != RBRACE && p->look->tag != 0) {
        struct Stmt *s = parser_stmt(p);

        if (result == Stmt_Null) {
            result = s;
        } else {
            result = (struct Stmt *)seq_new(result, s);
        }
    }

    return result;
}

struct Stmt *parser_stmt(struct Parser *p)
{
    struct Expr *x;
    struct Stmt *s1, *s2;

    if (!p->look || p->look->tag == 0 || p->look->tag == RBRACE)
        return Stmt_Null;

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

        case LBRACE:
            return parser_block(p);

        case RETURN:
            parser_match(p, RETURN);
            x = parser_bool(p);
            parser_match(p, SEMICOLON);
            return (struct Stmt *)return_new(x);

        default:
            if (p->look->tag == ID && p->look->lexeme) {
                const char *name = p->look->lexeme;

                struct NativeDecl *decl =
                hashmap_get(&native_decl_table, (void*)name);

                if (decl) {
                    struct Expr *e = parser_bool(p);
                    parser_match(p, SEMICOLON);
                    return (struct Stmt *)e;
                }
            }

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

        case STRING: {
            struct Expr *e = (struct Expr *)string_literal_new(p->look->lexeme);
            parser_move(p);
            return e;
        }

        case LPAREN: {
            parser_move(p);

            struct Type *ty = parser_type(p);
            if (ty != NULL) {
                parser_match(p, RPAREN);
                struct Expr *e = parser_unary(p);

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

                e->type = ty;
                return e;
            }

            x = parser_bool(p);
            parser_match(p, RPAREN);
            return x;
        }

        case NUM:
            x = (struct Expr *)constant_int(p->look->int_val);
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
            const char *name = p->look->lexeme;
            if (!name)
                parser_error(p, "identifier without lexeme");

            struct NativeDecl *decl =
                hashmap_get(&native_decl_table, (void*)name);

            if (decl) {
                parser_move(p);
                parser_match(p, LPAREN);

                struct Expr *args[4];
                int argc = 0;

                if (p->look->tag != RPAREN) {
                    while (1) {
                        args[argc++] = parser_bool(p);
                        if (p->look->tag == COMMA) {
                            parser_move(p);
                            continue;
                        }
                        break;
                    }
                }

                parser_match(p, RPAREN);

                return (struct Expr *)builtin_call_new(
                    name,
                    decl->id,
                    argc,
                    args
                );
            }

            struct Id *id = env_get_var(p->top, name);
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
