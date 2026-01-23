#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <limits.h>
#include "lexer.h"
#include "parser.h"
#include "ir.h"
#include "bpf_builder.h"
#include "cbpf.h"
#include "ir_lowering.h"

#define HOOK_SOCK_PATH "/tmp/ccbpf_hook.sock"

static int send_cmd(const char *cmd)
{
    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, HOOK_SOCK_PATH, sizeof(addr.sun_path) - 1);

    ssize_t n = sendto(fd, cmd, strlen(cmd), 0,
                       (struct sockaddr *)&addr, sizeof(addr));
    if (n < 0) {
        perror("sendto");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

static int compile_to_bpf(const char *src, const char *out)
{
    ir_init();

    struct lexer lex;
    lexer_init(&lex);
    lexer_set_input(src);

    struct Parser *p = parser_new(&lex);
    if (!p) {
        fprintf(stderr, "failed to create parser\n");
        return 1;
    }

    parser_program(p);

    struct bpf_builder b;
    bpf_builder_init(&b);

    struct ir_mes im;
    ir_mes_get(&im);

    ir_lower_program(im.ir_head, im.label_count, &b);

    struct bpf_insn *prog = bpf_builder_data(&b);
    int prog_len = bpf_builder_count(&b);

    write_ccbpf(out, prog, prog_len);
    printf("Wrote %s (%d instructions)\n", out, prog_len);

    bpf_builder_free(&b);
    return 0;
}

/* ============================================================
 *   1. compiler hello.c -o out.ccbpf
 *   2. attach hook_udp_input out.ccbpf
 *   3. detach hook_udp_input
 * ============================================================ */
int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage:\n");
        fprintf(stderr, "  %s <src.c> -o <out.ccbpf>\n", argv[0]);
        fprintf(stderr, "  %s attach <hook_name> <path-to-ccbpf>\n", argv[0]);
        fprintf(stderr, "  %s detach <hook_name>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "attach") != 0 &&
        strcmp(argv[1], "detach") != 0)
    {
        if (argc != 4 || strcmp(argv[2], "-o") != 0) {
            fprintf(stderr, "usage: %s <src.c> -o <out.ccbpf>\n", argv[0]);
            return 1;
        }

        const char *src = argv[1];
        const char *out = argv[3];
        return compile_to_bpf(src, out);
    }

    if (strcmp(argv[1], "attach") == 0) {
        if (argc != 4) {
            fprintf(stderr, "usage: %s attach <hook_name> <path>\n", argv[0]);
            return 1;
        }

        const char *hook = argv[2];
        const char *path = argv[3];

        char abs_path[256];
        if (!realpath(path, abs_path)) {
            perror("realpath");
            return 1;
        }

        char buf[512];
        snprintf(buf, sizeof(buf), "ATTACH %s %s", hook, abs_path);
        return send_cmd(buf);
    }

    if (strcmp(argv[1], "detach") == 0) {
        if (argc != 3) {
            fprintf(stderr, "usage: %s detach <hook_name>\n", argv[0]);
            return 1;
        }

        char buf[512];
        snprintf(buf, sizeof(buf), "DETACH %s", argv[2]);
        return send_cmd(buf);
    }

    fprintf(stderr, "unknown command: %s\n", argv[1]);
    return 1;
}
