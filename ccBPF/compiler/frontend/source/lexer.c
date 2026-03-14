#include "lexer.h"
#include "heap.h"
#include "inter.h"
#include "symbols.h"
#include <ctype.h>
#include <string.h>

#define LEXER_MAP_SIZE 16

mg_region_handle frontend_region;
mg_region_handle longterm_region;
mg_region_handle ir_region;

static const char *lexer_buf = NULL;
static size_t lexer_len = 0;
static size_t lexer_pos = 0;

void lexer_set_input_buffer(const char *buf, size_t len)
{
    lexer_buf = buf;
    lexer_len = len;
    lexer_pos = 0;
}

char reader_next_char(void)
{
    if (!lexer_buf || lexer_pos >= lexer_len)
        return '\0';

    return lexer_buf[lexer_pos++];
}

static struct lexer_token *new_lexer_token_word(const char *lexeme, int tag)
{
    struct lexer_token *t = mg_region_alloc(longterm_region, sizeof(*t));
    t->tag = tag;

    size_t n = strlen(lexeme) + 1;
    char *s = mg_region_alloc(longterm_region, n);
    memcpy(s, lexeme, n);

    t->lexeme = s;
    return t;
}

static struct lexer_token *new_lexer_token_num(int v)
{
    struct lexer_token *t = mg_region_alloc(longterm_region, sizeof(*t));
    if (!t)
        return NULL;
    t->tag = NUM;
    t->int_val = v;
    return t;
}

static struct lexer_token *new_lexer_token_char(int tag, char ch)
{
    struct lexer_token *t = mg_region_alloc(longterm_region, sizeof(*t));
    if (!t)
        return NULL;
    t->tag = tag;
    t->ch = ch;
    return t;
}

void lexer_reserve(struct lexer *lex, const char *lexeme, int tag)
{
    hashmap_put(&lex->words, (void *)lexeme, (void *)(intptr_t)tag);
}

static void readch(struct lexer *lex)
{
    lex->peek = reader_next_char();
}

void lexer_init(struct lexer *lex)
{
    hashmap_init(&lex->words, LEXER_MAP_SIZE, HASHMAP_KEY_STRING);

    lex->line = 1;
    lex->peek = ' ';

    lexer_reserve(lex, "if", IF);
    lexer_reserve(lex, "else", ELSE);
    lexer_reserve(lex, "true", TRUE);
    lexer_reserve(lex, "false", FALSE);

    lexer_reserve(lex, "int", BASIC);
    lexer_reserve(lex, "bool", BASIC);
    lexer_reserve(lex, "char", BASIC);
    lexer_reserve(lex, "short", BASIC);
    lexer_reserve(lex, "unsigned", BASIC);
    lexer_reserve(lex, "return", RETURN);

    lexer_reserve(lex, "struct", STRUCT);
}

void compiler_init(uint8_t region_bit, uint32_t cap, uint32_t ir_cap)
{
    frontend_region = mg_region_create_pool(region_bit);
    longterm_region = mg_region_create_bump(cap);
    ir_region = mg_region_create_bump(ir_cap);
    init_stmt_singletons();
    init_constant_singletons();
}

void frontend_destroy(struct lexer *lex)
{
    hashmap_destroy(&lex->words);
    symbol_destroy();
    mg_region_destroy(frontend_region);
    mg_region_destroy(longterm_region);
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
        case '"': {
            char buf[64];
            int i = 0;

            readch(lex);

            while (lex->peek != '"' && lex->peek != '\0') {
                if (i < 64)
                    buf[i++] = lex->peek;
                readch(lex);
            }

            buf[i] = '\0';

            readch(lex);

            struct lexer_token *t = mg_region_alloc(frontend_region, sizeof(*t));
            t->tag = STRING;
            t->lexeme = region_strdup(buf);
            return t;
        }

        case '&':
            if (readch_match(lex, '&')) return new_lexer_token_char(AND, '&');
            return new_lexer_token_char(AND_BIT, '&');

        case '|':
            if (readch_match(lex, '|')) return new_lexer_token_char(OR, '|');
            return new_lexer_token_char(OR_BIT, '|');

        case '=':
            if (readch_match(lex, '=')) return new_lexer_token_char(EQ, '=');
            return new_lexer_token_char(ASSIGN, '=');

        case '!':
            if (readch_match(lex, '=')) return new_lexer_token_char(NE, '!');
            return new_lexer_token_char(NOT, '!');

        case '<':
            if (readch_match(lex, '=')) return new_lexer_token_char(LE, '<');
            return new_lexer_token_char(LT, '<');

        case '>':
            if (readch_match(lex, '=')) return new_lexer_token_char(GE, '>');
            return new_lexer_token_char(GT, '>');

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
            readch(lex);
            if (lex->peek == '-') {
                readch(lex);
                return new_lexer_token_char(DEC, '-');
            } else if (lex->peek == '=') {
                readch(lex);
                return new_lexer_token_char(SUB_ASSIGN, '-');
            } else if (lex->peek == '>') {
                readch(lex);
                return new_lexer_token_char(ARROW, '-');
            }

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

    if (lex->peek == '0') {
        readch(lex);
        if (lex->peek == 'x' || lex->peek == 'X') {
            readch(lex);
            int v = 0;
            while (isxdigit((unsigned char)lex->peek)) {
                char c = lex->peek;
                readch(lex);

                if (c >= '0' && c <= '9') v = v * 16 + (c - '0');
                else if (c >= 'a' && c <= 'f') v = v * 16 + (c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') v = v * 16 + (c - 'A' + 10);
            }
            return new_lexer_token_num(v);
        }
        return new_lexer_token_num(0);
    }

    if (isdigit((unsigned char)lex->peek)) {
        int v = 0;
        do {
            v = 10 * v + (lex->peek - '0');
            readch(lex);
        } while (isdigit((unsigned char)lex->peek));

        return new_lexer_token_num(v);
    }

    if (isalpha((unsigned char)lex->peek) || lex->peek == '_') {
        char buf[64];
        int i = 0;

        do {
            if (i < 64)
                buf[i++] = lex->peek;
            readch(lex);
        } while (isalnum((unsigned char)lex->peek) || lex->peek == '_');

        buf[i] = '\0';

        int tag = (int)(intptr_t)hashmap_get(&lex->words, buf);
        if (tag != 0)
            return new_lexer_token_word(buf, tag);

        return new_lexer_token_word(buf, ID);
    }

    char ch = lex->peek;
    readch(lex);
    return new_lexer_token_char(ch, ch);
}
