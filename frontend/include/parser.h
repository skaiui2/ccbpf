#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "symbols.h"
#include "inter.h"

struct Parser {
    struct lexer       *lex;
    struct lexer_token *look;
    struct Env         *top;
    int                 used;
};

char *token_to_string(struct lexer_token *tok);

struct Parser *parser_new(struct lexer *lex);

void           parser_program(struct Parser *p);
struct Stmt   *parser_block(struct Parser *p);
void           parser_decls(struct Parser *p);
struct Type   *parser_type(struct Parser *p);
struct Type   *parser_dims(struct Parser *p, struct Type *p0);
struct Stmt   *parser_stmts(struct Parser *p);
struct Stmt   *parser_stmt(struct Parser *p);
struct Stmt   *parser_assign(struct Parser *p);
struct Expr   *parser_bool(struct Parser *p);
struct Expr   *parser_join(struct Parser *p);
struct Expr   *parser_rel(struct Parser *p);
struct Expr   *parser_expr(struct Parser *p);
struct Expr   *parser_term(struct Parser *p);
struct Expr   *parser_unary(struct Parser *p);
struct Expr   *parser_factor(struct Parser *p);
struct Access *parser_offset(struct Parser *p, struct Id *a);

#endif
