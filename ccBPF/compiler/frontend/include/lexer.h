#ifndef LEXER_H
#define LEXER_H

#include "hashmap.h"
#include "mg_alloc.h"

extern mg_region_handle frontend_region;
extern mg_region_handle longterm_region;
extern mg_region_handle ir_region;

enum tag {
    AND = 256,     // &&
    BASIC,         // int, short, bool, char
    ELSE,
    AND_BIT,       // & 
    OR_BIT,        // | 
    LT,            // < 
    GT,            // >
    EQ,            // ==
    FALSE,
    GE,            // >=
    ID,
    IF,
    INDEX,
    LE,            // <=
    NE,            // !=
    NUM,           // integer literal
    OR,            // ||
    STRING,

    TEMP,
    TRUE,
    ARROW,

    NOT,
    // function / block symbols
    LPAREN,        // (
    RPAREN,        // )
    LBRACE,        // {
    RBRACE,        // }
    COMMA,         // ,
    SEMICOLON,     // ;

    RETURN,        // return
    ENUM,          // enum
    STRUCT, 

    PLUS,       // +
    MINUS,      // -
    STAR,       // *
    SLASH,      // /
    MOD,        // %

    ASSIGN,     // =
    ADD_ASSIGN, // +=
    SUB_ASSIGN, // -=
    MUL_ASSIGN, // *=
    DIV_ASSIGN, // /=

    INC,        // ++
    DEC,        // --

    DOT,        // .
    LBRACKET,   // [
    RBRACKET    // ]
};

struct lexer_token {
    int tag;
    int line;
    int int_val;
    float real_val;
    char *lexeme;
    char ch;
};

struct lexer {
    int line;
    char peek;
    char *filename;      
    struct hashmap words;
};

void compiler_init(uint8_t region_bit, uint32_t cap, uint32_t ir_cap);
void lexer_init(struct lexer *lex);
void frontend_destroy(struct lexer *lex);
struct lexer_token *lexer_scan(struct lexer *lex);
void lexer_reserve(struct lexer *lex, const char *lexeme, int tag);
void lexer_set_input_buffer(const char *buf, size_t len);

#endif
