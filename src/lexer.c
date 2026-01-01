#include "lexer.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static FILE *lexer_input = NULL;

void lexer_set_input(const char *filename)
{
    lexer_input = fopen(filename, "r");
    if (!lexer_input) {
        perror("Cannot open input file");
        exit(1);
    }
}

char reader_next_char(void)
{
    if (!lexer_input)
        return '\0';

    int c = fgetc(lexer_input);
    if (c == EOF)
        return '\0';

    return (char)c;
}

static struct lexer_token *new_lexer_token_word(const char *lexeme, int tag)
{
    struct lexer_token *t = malloc(sizeof(struct lexer_token));
    if (!t)
        return NULL;
    t->tag = tag;
    t->lexeme = strdup(lexeme);
    return t;
}

static struct lexer_token *new_lexer_token_num(int v)
{
    struct lexer_token *t = malloc(sizeof(struct lexer_token));
    if (!t)
        return NULL;
    t->tag = NUM;
    t->int_val = v;
    return t;
}

static struct lexer_token *new_lexer_token_real(float v)
{
    struct lexer_token *t = malloc(sizeof(struct lexer_token));
    if (!t)
        return NULL;
    t->tag = REAL;
    t->real_val = v;
    return t;
}

static struct lexer_token *new_lexer_token_char(int tag, char ch)
{
    struct lexer_token *t = malloc(sizeof(struct lexer_token));
    if (!t)
        return NULL;
    t->tag = tag;
    t->ch = ch;
    return t;
}

void lexer_reserve(struct lexer *lex, const char *lexeme, int tag)
{
    struct lexer_token *w = new_lexer_token_word(lexeme, tag);
    hashmap_put(&lex->words, w->lexeme, w);
}


static void readch(struct lexer *lex)
{
    lex->peek = reader_next_char();
}

void lexer_init(struct lexer *lex)
{
    lex->line = 1;
    lex->peek = ' ';

    hashmap_init(&lex->words, 128, HASHMAP_KEY_STRING);

    lexer_reserve(lex, "if", IF);
    lexer_reserve(lex, "else", ELSE);
    lexer_reserve(lex, "while", WHILE);
    lexer_reserve(lex, "do", DO);
    lexer_reserve(lex, "break", BREAK);
    lexer_reserve(lex, "true", TRUE);
    lexer_reserve(lex, "false", FALSE);

    lexer_reserve(lex, "int", BASIC);
    lexer_reserve(lex, "bool", BASIC);
    lexer_reserve(lex, "char", BASIC);
    lexer_reserve(lex, "float", BASIC);
    lexer_reserve(lex, "return", RETURN);
}

static int readch_match(struct lexer *lex, char c)
{
    readch(lex);
    if (lex->peek != c)
        return 0;
    lex->peek = ' ';
    return 1;
}

struct lexer_token *lexer_scan(struct lexer *lex)
{
    for (;; readch(lex)) {
        if (lex->peek == ' ' || lex->peek == '\t') {
            continue;
        }
        else if (lex->peek == '\n') {
            lex->line++;
        } else {
            break;
        }
    }

    switch (lex->peek) {
        case '&':
            if (readch_match(lex, '&')) return new_lexer_token_char(AND, '&');
            return new_lexer_token_char('&', '&');

        case '|':
            if (readch_match(lex, '|')) return new_lexer_token_char(OR, '|');
            return new_lexer_token_char('|', '|');
        
        case '=':
            if (readch_match(lex, '=')) return new_lexer_token_char(EQ, '=');
            return new_lexer_token_char(ASSIGN, '=');

        case '!':
            if (readch_match(lex, '=')) return new_lexer_token_char(NE, '!');
            return new_lexer_token_char(NOT, '!');

        case '<':
            if (readch_match(lex, '=')) return new_lexer_token_char(LE, '<');
            return new_lexer_token_char('<', '<');

        case '>':
            if (readch_match(lex, '=')) return new_lexer_token_char(GE, '>');
            return new_lexer_token_char('>', '>');

        case '(':
            readch(lex);
            return new_lexer_token_char(LPAREN, '(');

        case ')':
            readch(lex);
            return new_lexer_token_char(RPAREN, ')');

        case '{':
            readch(lex);
            return new_lexer_token_char(LBRACE, '{');

        case '}':
            readch(lex);
            return new_lexer_token_char(RBRACE, '}');

        case ',':
            readch(lex);
            return new_lexer_token_char(COMMA, ',');

        case ';':
            readch(lex);
            return new_lexer_token_char(SEMICOLON, ';');

        case '+':
            if (readch_match(lex, '+')) return new_lexer_token_char(INC, '+');
            if (lex->peek == '=') return new_lexer_token_char(ADD_ASSIGN, '+');
            return new_lexer_token_char(PLUS, '+');

        case '-':
            if (readch_match(lex, '-')) return new_lexer_token_char(DEC, '-');
            if (lex->peek == '=') return new_lexer_token_char(SUB_ASSIGN, '-');
            return new_lexer_token_char(MINUS, '-');

        case '*':
            if (readch_match(lex, '=')) return new_lexer_token_char(MUL_ASSIGN, '*');
            return new_lexer_token_char(STAR, '*');

        case '/':
            if (readch_match(lex, '=')) return new_lexer_token_char(DIV_ASSIGN, '/');
            return new_lexer_token_char(SLASH, '/');

        case '%':
            readch(lex);
            return new_lexer_token_char(MOD, '%');

        case '.':
            readch(lex);
            return new_lexer_token_char(DOT, '.');

        case '[':
            readch(lex);
            return new_lexer_token_char(LBRACKET, '[');

        case ']':
            readch(lex);
            return new_lexer_token_char(RBRACKET, ']');

    }

    if (isdigit((unsigned char)lex->peek)) {
        int v = 0;
        do {
            v = 10 * v + (lex->peek - '0');
            readch(lex);
        } while (isdigit((unsigned char)lex->peek));

        if (lex->peek != '.')
            return new_lexer_token_num(v);

        float x = (float)v;
        float d = 10.0f;

        for (;;) {
            readch(lex);
            if (!isdigit((unsigned char)lex->peek)) break;
            x += (float)(lex->peek - '0') / d;
            d *= 10.0f;
        }

        return new_lexer_token_real(x);
    }

    if (isalpha((unsigned char)lex->peek)) {
        char buf[256];
        int i = 0;

        do {
            if (i < (int)sizeof(buf) - 1)
                buf[i++] = lex->peek;
            readch(lex);
        } while (isalnum((unsigned char)lex->peek));

        buf[i] = '\0';

        struct lexer_token *w = hashmap_get(&lex->words, buf);
        if (w != NULL)
            return w;

        w = new_lexer_token_word(buf, ID);
        hashmap_put(&lex->words, w->lexeme, w);
        return w;
    }

    struct lexer_token *tok = new_lexer_token_char(lex->peek, lex->peek);
    lex->peek = ' ';
    return tok;
}
