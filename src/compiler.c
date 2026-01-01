#include<stdio.h>
#include<stdlib.h>
#include<memory.h>
#include<unistd.h>
#include<fcntl.h>
#include "lexer.h"
#include "parser.h"

int main(int argc, char **argv)
{
    const char *filename = "../hello.c";
    if (argc > 1) {
        filename = argv[1];
    }

    struct lexer lex;
    lexer_init(&lex);
    lexer_set_input(filename);

    struct Parser *p = parser_new(&lex);
    if (!p) {
        fprintf(stderr, "failed to create parser\n");
        return 1;
    }

    /* 这里会从 hello.c 读入、词法分析、语法分析并生成中间代码 */
    parser_program(p);

    return 0;
}
